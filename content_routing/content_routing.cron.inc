<?php

// $Id: content_routing.cron.inc,v 1.1 2008/12/04 11:23:40 ralph Exp $


/**
 * Implementation of hook_cron
 * To save a bit of memory, and keep this file small, we include
 * an external file and call into that.
 * @param none
 * @return none
 */
function _content_routing_cron() {
  $start_time = time();

  $last_timestamp = variable_get('content_routing_last_timestamp', 0);
  $nodes_per_run = variable_get('content_routing_nodes_per_run', 10);

  $stop_words = content_routing_get_stop_words();

  $processed_nodes = '';

  $node_count = 0;

  error_log("CR starting on timestamp $last_timestamp (" . (time() - $last_timestamp) . " to go) and working on $nodes_per_run nodes");

  $result = db_query("SELECT nid from {node} WHERE changed > %d ORDER BY changed ASC LIMIT %d", $last_timestamp, $nodes_per_run);

  while($row = db_fetch_object($result)) {
    $node = node_load($row->nid);
    _content_routing_do_routing($node, $stop_words);

    $node_count++;
    $processed_nodes .= $node->nid . ' ';

    // Save the last timestamp we've processed so that we don't do it
    // again next time
    variable_set('content_routing_last_timestamp', $node->changed);
  }

  // Now see if there are any 'spare' nodes that we could process
  // If there are, then we process old nodes that we've already routed.
  // This makes sure all nodes are basically up to date with new content
  // and comments etc.
  if($node_count < $nodes_per_run) {
    $min_age = time() - variable_get('content_routing_min_age', 0);
    // Some 'spare nodes' available for processing
    $result = db_query("SELECT DISTINCT(csid) FROM {content_routing_links} WHERE ctype='node' AND lastupdate < %d ORDER BY lastupdate ASC LIMIT %d", $min_age, ($nodes_per_run - $node_count));
    $processed_nodes .= '> ';
    while($row = db_fetch_object($result)) {
      $node = node_load($row->csid);
      _content_routing_do_routing($node, $stop_words);
      $processed_nodes .= $node->nid . ' ';
    }
  }
  error_log("CR processed nodes: " . $processed_nodes . " in " . (time() - $start_time) . " seconds");
}

function _content_routing_do_routing(&$node, &$stop_words) {

  // Now get any extras (like comments)
  $info = node_invoke_nodeapi($node, 'update index');
  if(is_array($info)) {
    $info = implode("\n", $info);
  }

  // Now concatenate the text
  $text = strtolower(strip_tags(check_markup($node->body, $node->format, FALSE) . $info));

  // We don't need our objects anymore - clear the RAM for them
  $info = NULL;

  // Now strip the text down...
  $words = content_routing_get_text($text, $stop_words);

  // Now score the content by node - this uses the search database
  $scores = array();

  $score_drop_pc = variable_get('content_routing_score_drop_pc', 50);

  foreach($words as $word) {
    $top_score = NULL;
    $drop_score = NULL;
    $index_result = db_query("SELECT sid, score FROM {search_index} WHERE word='%s' AND type='node' AND sid!=%d ORDER BY score DESC", $word, $node->nid);
    $saved_rows = array();
    $row_count = 0;
    while($index_row = db_fetch_object($index_result)) {
      if($top_score == NULL) {
        $top_score = $index_row->score;
        $drop_score = ($top_score / 100) * $score_drop_pc;
      }
      if(isset($drop_score) && $index_row->score < $drop_score) {
        // If we've dropped below a useful score, then we stop even looking at
        // these nodes - their relative score is so low that there's no point
        // dragging yet more data from the DB
        break;
      }
      $saved_rows[] = $index_row;
      $row_count ++;
    }

    foreach($saved_rows as $index_row) {
      $score = $index_row->score / $row_count;
      if(isset($scores[$index_row->sid])) {
        $scores[$index_row->sid] = $scores[$index_row->sid] + $score;
      } else {
        $scores[$index_row->sid] = $score;
      }
    }
      
  }

  // Now we've got scores this node's content in other nodes
  // Pick the top "n" nodes and store them in our CR table
  arsort($scores, SORT_NUMERIC);

  // Remove old CR documents
  db_query("DELETE FROM {content_routing_links} WHERE csid=%d AND ctype='node'", $node->nid);

  // Now, for the top "n" documents, store them in our CR table...
  $limit = variable_get('content_routing_documents', 10);
  foreach(array_keys($scores) as $doc) {
    $limit--;
    if($limit <= 0) {
      break;
    }
    //error_log("Node: " . $node->nid . " CR node: " . $doc . " with score " . $scores[$doc]);
    db_query("INSERT INTO {content_routing_links} (csid,ctype,lastupdate,rsid,rtype,score) VALUES (%d,'node',%d,%d,'node',%f)", $node->nid, time(), $doc, $scores[$doc]);
  }

  return;
}

function content_routing_get_stop_words() {
  // In order to save an expensive DB query, we cache stop words for a period
  // of time. We do this via Drupal variable
  $sw_timestamp = variable_get('content_routing_stopwords_timestamp', 0);
  $sw_max_age = variable_get('content_routing_stopwords_max_age', 0);

  if((time() - $sw_max_age) > $sw_timestamp) {
    // Stop words are too old - refresh them
  
    $stop_pc = variable_get('content_routing_stopwords_percentage', 75);
    $total_docs = db_result(db_query("SELECT COUNT(DISTINCT(sid)) FROM {search_index} WHERE type='node'"));

    $max_docs = (integer)(($total_docs/100) * $stop_pc);

    error_log("stop_pc=$stop_pc total_docs=$total_docs max_docs=$max_docs");

    $stop_words = array();
    $result = db_query("SELECT COUNT(sid) AS cnt, word FROM {search_index} WHERE type='node' GROUP BY word ORDER BY cnt DESC");
    while($row = db_fetch_object($result)) {
      if($row->cnt < $max_docs) {
        break;
      }
      $stop_words[] = $row->word;
    }
    variable_set('content_routing_stopwords', $stop_words);
    variable_set('content_routing_stopwords_timestamp', time());
  } else {
    // Just get them from cache
    $stop_words = variable_get('content_routing_stopwords', array());
  }

  return $stop_words;
}

function content_routing_get_text(&$text, &$stop_words) {

  // This is a search.module variable. Things less than this size aren't stored
  // in the search index, so there's no point us using them either
  $min = variable_get('minimum_word_size', 3);

  $out = array();

  foreach(preg_split('/\s+/', $text) as $word) {
    // Skip short words.
    if(drupal_strlen($word) < $min) {
      continue;
    }
    if(!in_array($word, $stop_words)) {
      // Not a stop word - so keep it
      $out[] = $word;
    }
  }
  return $out;
}

// The following line is for Vim users - please don't delete it.
// vim: set filetype=php expandtab tabstop=2 shiftwidth=2:
