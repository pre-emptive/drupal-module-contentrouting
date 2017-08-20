<?php

// $Id: content_routing.admin.inc,v 1.1 2008/12/04 11:23:40 ralph Exp $

/**
 * Function to return a rendered system settings form for the block configuration
 * suitable for drupal_get_form() use.
 * @param none
 * @return rendered HTML
 */
function content_routing_admin_settings_form() {
  $form = array();

  $nodes_per_run = array(1 => 1, 5 => 5, 10 => 10, 20 => 20, 50 => 50, 100 => 100);
  $form['content_routing_nodes_per_run'] = array(
    '#type' => 'select',
    '#title' => t('Documents to process per cron run'),
    '#description' => t('The maximum number of items indexed in each pass of a cron maintenance task. If necessary, reduce the number of items to prevent timeouts and memory errors while processing.'),
    '#options' => $nodes_per_run,
    '#default_value' => variable_get('content_routing_nodes_per_run', 10),
  );

  $form['content_routing_documents'] = array(
    '#type' => 'textfield',
    '#size' => 5,
    '#title' => t('Number of CR links to store'),
    '#description' => t('The number of related documents to store when processing content. Larger numbers take longer to process and maintain.'),
    '#default_value' => variable_get('content_routing_documents',10),
  );

  // This should be in the block configuration screen, not here
  $form['content_routing_display_docs'] = array(
    '#type' => 'textfield',
    '#size' => 5,
    '#title' => t('Number of documents to display'),
    '#description' => t('The number of documents to display in the Related Content block. This cannot be higher than the number of CR links stored.'),
    '#default_value' => variable_get('content_routing_display_docs',10),
  );

  $ages = drupal_map_assoc(array(0, 3600, 28800, 86400, 604800), 'format_interval');
  $form['content_routing_min_age'] = array(
    '#type' => 'select',
    '#title' => t('Minimum age of CR links'),
    '#description' => t('The minimum age CR content can be before it is regenerated during cron runs. This prevents CR links being recreated more often than necessary. Set this higher if changes are infrequent on the site.'),
    '#options' => $ages,
    '#default_value' => variable_get('content_routing_min_age', 10),
  );

  $percentages = array(0 => 0, 10 => 10, 25 => 25, 50 => 50, 75 => 75, 90 => 90);
  $form['content_routing_score_drop_pc'] = array(
    '#type' => 'select',
    '#title' => t('Stop Looking on Score'),
    '#description' => t('When processing documents, the processor can stop looking for further documents when the word score drops below this percentage. This is a processing optimisation; lower percentages require more processing but provide more accurate results.'),
    '#options' => $percentages,
    '#default_value' => variable_get('content_routing_score_drop_pc', 50),
  );

  $sw_percentages = array(10 => 10, 25 => 25, 50 => 50, 75 => 75, 90 => 90);
  $form['content_routing_stopwords_percentage'] = array(
    '#type' => 'select',
    '#title' => t('Stopwords Percentage'),
    '#description' => t('The percentage of documents a word can appear in before it is considered a stopword. Common words are stripped when considering content, which speeds up processing, possibly at the expense of some accuracy.'),
    '#options' => $sw_percentages,
    '#default_value' => variable_get('content_routing_stopwords_percentage', 75),
  );

  $stopword_ages = drupal_map_assoc(array(0, 86400, 604800, 1209600, 2419200), 'format_interval');
  $form['content_routing_stopwords_max_age'] = array(
    '#type' => 'select',
    '#title' => t('Stopwords Cache Age'),
    '#description' => t('The longest stopwords are held in cache before being re-collected. This optimisation reduces the (noticeable) load imposed by collecting stopwords.'),
    '#options' => $stopword_ages,
    '#default_value' => variable_get('content_routing_stopwords_max_age', 86400),
  );


  return system_settings_form($form);
}

// The following line is for Vim users - please don't delete it.
// vim: set filetype=php expandtab tabstop=2 shiftwidth=2:
