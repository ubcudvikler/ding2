<?php
/**
 * @file
 * Panels render to display search results.
 */

use Ting\Search\TingSearchSort;

$plugin = array(
  'single' => TRUE,
  'title' => t('Ting search - Search results'),
  'description' => t('The results of a search using keywords.'),
  'required context' => new ctools_context_required(t('Keywords'), 'string'),
  'category' => t('Widgets'),
  'content_types' => array('search_result'),
  'defaults' => array(
    'type' => 'node',
    'log' => TRUE,
    'override_empty' => FALSE,
    'empty_title' => '',
    'empty' => '',
    'empty_format' => filter_fallback_format(),
    'override_no_key' => FALSE,
    'no_key_title' => '',
    'no_key' => '',
    'no_key_format' => filter_fallback_format(),
  ),
  'render first' => TRUE,
  'render last' => FALSE,
);

/**
 * Render the custom content type.
 */
function ting_search_search_result_content_type_render($subtype, $conf, $panel_args, $context) {
  $search_info = search_get_info();
  if (empty($search_info[$conf['type']])) {
    return;
  }
  $info = $search_info[$conf['type']];

  $keys = NULL;
  if (!empty($context) && isset($context->data)) {
    $keys = $context->data;
  }

  $conditions = NULL;
  if (isset($info['conditions_callback']) && function_exists($info['conditions_callback'])) {
    // Build an optional array of more search conditions.
    $conditions = $info['conditions_callback']($keys);
  }

  // Display nothing at all if no keywords were entered.
  if (empty($keys) && empty($conditions)) {
    if (!empty($conf['override_no_key'])) {
      $block = new stdClass();
      $block->title = $conf['no_key_title'];
      $block->content = check_markup($conf['no_key'], $conf['no_key_format'], FALSE);
      return $block;
    }
    return;
  }

  // Build the content type block.
  $block = new stdClass();
  $block->module = 'search';
  $block->delta = 'result';

  $suffix = $prefix = $results = '';

  // Only search if there are keywords or non-empty conditions.
  if ($keys || !empty($conditions)) {
    // Collect the search results.
    $results = search_data($keys, $info['module'], $conditions);
    $search_result = ting_search_current_results();
    $prefix = implode(module_invoke_all('ting_search_results_prefix', $keys, $conditions, $search_result));
    $suffix = implode(module_invoke_all('ting_search_results_suffix', $keys, $conditions, $search_result));
  }

  if (!empty($conf['log'])) {
    // Log the search keys:
    watchdog('search', 'Searched %type for %keys.', array('%keys' => $keys, '%type' => $info['title']), WATCHDOG_NOTICE, l(t('results'), $_GET['q']));
  }

  if (!empty($results['#results'])) {
    // Get message limit and raw data well search results.
    $limit = variable_get('ting_search_result_message_limit', 100);

    // Set message.
    $message = '';

    // If it does not seem like the user picked a sort but our query has one,
    // tell the user that we have sorted the result.
    if (isset($search_result) && empty($conditions['sort']) && !empty($search_result->getSearchRequest()->getSorts())) {
      $provider_sort_options = ting_search_sort_options();

      $mapper = function(TingSearchSort $sort) use ($provider_sort_options) {
        return isset($provider_sort_options[$sort->getField()]['label_short']) ? $provider_sort_options[$sort->getField()]['label_short'] : NULL;
      };
      $labels = array_filter(array_map($mapper, $search_result->getSearchRequest()->getSorts()));

      $labels = implode(', ', $labels);
      $msg = t('Records are sorted by %sortUsed . Select another sort if it is not suited', array('%sortUsed' => $labels));
      $message .= '<div class="messages status">' . $msg . '</div>';
    }


    if (isset($search_result) && $limit < $search_result->getNumTotalObjects()) {
      $msg = t('Your search gave more than %s results. Try to search more specific or use the facets to filter the result.');
      $msg = sprintf($msg, $limit);
      $message .= '<div class="messages search-field-in-content--message">' . $msg . '</div>';
    }

    $prefix .= $message;

    $block->title = t('Search results');
    $block->content = $prefix . theme('ting_search_results', array('results' => $results['#results'], 'module' => $conf['type'])) . $suffix;
  }
  else {
    if (empty($conf['override_empty'])) {
      $block->title = t('Your search yielded no results');
      $block->content = search_help('search#noresults', drupal_help_arg());
    }
    else {
      $block->title = $conf['empty_title'];
      $block->content = check_markup($conf['empty'], $conf['empty_format'], FALSE);
    }
  }

  return $block;
}

/**
 * Returns an edit form for custom type settings.
 */
function ting_search_search_result_content_type_edit_form($form, &$form_state) {
  $conf = $form_state['conf'];

  $types = array();
  foreach (search_get_info() as $module => $info) {
    $types[$module] = $info['title'];
  }

  $form['type'] = array(
    '#type' => 'select',
    '#title' => t('Search type'),
    '#options' => $types,
    '#default_value' => $conf['type'],
  );

  $form['log'] = array(
    '#type' => 'checkbox',
    '#default_value' => $conf['log'],
    '#title' => t('Record a watchdog log entry when searches are made'),
  );

  $form['override_empty'] = array(
    '#type' => 'checkbox',
    '#default_value' => $conf['override_empty'],
    '#title' => t('Override "no result" text'),
  );

  $form['empty_title'] = array(
    '#title' => t('Title'),
    '#type' => 'textfield',
    '#default_value' => $conf['empty_title'],
    '#dependency' => array('edit-override-empty' => array(1)),
  );

  $form['empty_field'] = array(
    '#type' => 'text_format',
    '#title' => t('No result text'),
    '#default_value' => $conf['empty'],
    '#format' => $conf['empty_format'],
    '#dependency' => array('edit-override-empty' => array(1)),
  );

  $form['override_no_key'] = array(
    '#type' => 'checkbox',
    '#default_value' => $conf['override_no_key'],
    '#title' => t('Display text if no search keywords were submitted'),
  );

  $form['no_key_title'] = array(
    '#title' => t('Title'),
    '#type' => 'textfield',
    '#default_value' => $conf['no_key_title'],
    '#dependency' => array('edit-override-no-key' => array(1)),
  );

  $form['no_key_field'] = array(
    '#type' => 'text_format',
    '#title' => t('No result text'),
    '#default_value' => $conf['no_key'],
    '#format' => $conf['no_key_format'],
    '#dependency' => array('edit-override-no-key' => array(1)),
  );

  return $form;
}

/**
 * Submit handler for search form.
 */
function ting_search_search_result_content_type_edit_form_submit($form, &$form_state) {
  // Copy the text_format values over to where we normally store them.
  $form_state['values']['empty'] = $form_state['values']['empty_field']['value'];
  $form_state['values']['empty_format'] = $form_state['values']['empty_field']['format'];
  $form_state['values']['no_key'] = $form_state['values']['no_key_field']['value'];
  $form_state['values']['no_key_format'] = $form_state['values']['no_key_field']['format'];
  // Copy everything from our defaults.
  foreach (array_keys($form_state['plugin']['defaults']) as $key) {
    $form_state['conf'][$key] = $form_state['values'][$key];
  }
}

/**
 * Returns the administrative title for a type.
 */
function ting_search_search_result_content_type_admin_title($subtype, $conf, $context) {
  $info = search_get_info();
  $type = isset($info[$conf['type']]['title']) ? $info[$conf['type']]['title'] : t('Missing/broken type');
  return t('@type search result', array('@type' => $type));
}
