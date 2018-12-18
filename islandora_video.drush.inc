<?php

/**
 * @file
 * Drush command/hook implementation for updating existing video objects.
 */

/**
 * Implements hook_drush_command().
 */
function islandora_video_drush_command() {
  $commands = array();

  $commands['islandora-video-update-videos'] = array(
    'description' => dt('Update existing videos to remove OGG derivatives as they are no longer needed.'),
    'drupal dependencies' => array(
      'islandora',
      'islandora_video',
    ),
    'options' => array(),
    'examples' => array(
      'drush -u 1 islandora-video-update-videos' => dt('Removing OGGs from existing video objects.'),
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );
  $commands['islandora-video-generate-tn-large'] = array(
    'description' => dt('Alters namespace-accessible video objects that have a TN datastream but no TN_LARGE datastream, by copying the TN to TN_LARGE, triggering derivatives to resize TN_LARGE to create a smaller TN. This script will set local generation of video thumbnails to TRUE and will set the video solution pack to use TN_LARGE for full-size thumbnails. Before running this script, ensure that derivatives are not deferred in Islandora configuration.'),
    'drupal dependencies' => array(
      'islandora',
      'islandora_video',
    ),
    'options' => array(),
    'examples' => array(
      'drush -u 1 islandora-video-use-tn-large' => dt('Creating TN_LARGE from TN and scaling down TN'),
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );
  return $commands;
}

/**
 * Command callback to update citations with PDF derivatives.
 */
function drush_islandora_video_update_videos() {
  batch_set(islandora_video_update_videos_create_batch());
  drush_backend_batch_process();
}

/**
 * Constructs a batch used to update things via Drush.
 */
function islandora_video_update_videos_create_batch() {
  return array(
    'operations' => array(
      array('islandora_video_update_videos_batch_operation', array()),
    ),
    'title' => t('Removing OGG derivatives from existing video objects...'),
    'init_message' => t('Preparing to delete OGGs.'),
    'progress_message' => t('Time elapsed: @elapsed <br/>Estimated time remaining @estimate.'),
    'error_message' => t('An error has occurred.'),
    'file' => drupal_get_path('module', 'islandora_video') . '/islandora_video.drush.inc',
  );
}

/**
 * Constructs and performs the citation batch operation.
 *
 * @param array $context
 *   The context of the Drupal batch.
 */
function islandora_video_update_videos_batch_operation(&$context) {
  $update_offset = 25;
  $base_query = <<<EOQ
SELECT ?pid ?date FROM <#ri>
WHERE {
  ?pid <fedora-model:hasModel> <info:fedora/islandora:sp_videoCModel> ;
       <fedora-view:disseminates> ?ds .
  ?ds <fedora-view:disseminationType> <info:fedora/*/OGG> ;
      <fedora-view:lastModifiedDate> ?date
}
ORDER BY ?date
EOQ;
  $sandbox = &$context['sandbox'];
  if (isset($sandbox['last_date'])) {
    $last_date = $sandbox['last_date'];
    if ($last_date) {
      $operations_query = <<<EOQ
      SELECT ?pid ?date FROM <#ri>
    WHERE {
      ?pid <fedora-model:hasModel> <info:fedora/islandora:sp_videoCModel> ;
           <fedora-view:disseminates> ?ds .
      ?ds <fedora-view:disseminationType> <info:fedora/*/OGG> ;
          <fedora-view:lastModifiedDate> ?date
      FILTER(?date > '$last_date'^^xsd:dateTime)
    }
    ORDER BY ?date
    LIMIT $update_offset
EOQ;
    }
  }
  $connection = islandora_get_tuque_connection();

  if (!isset($sandbox['total'])) {
    $sparql_count = $connection->repository->ri->countQuery($base_query, 'sparql');
    $sandbox['total'] = $sparql_count;
    if ($sandbox['total'] === 0) {
      return;
    }
  }

  if (isset($sandbox['last_date'])) {
    $results = $connection->repository->ri->sparqlQuery($operations_query);

    $context['message'] = t('Processing results after @date.', array(
      '@date' => $sandbox['last_date'],
    ));
  }
  else {
    $context['message'] = t('Processing initial results.', array(
      '@date' => $sandbox['last_date'],
    ));
    $base_query .= <<<EOQ
  LIMIT $update_offset
EOQ;

    $results = $connection->repository->ri->sparqlQuery($base_query);
  }
  $last_date = FALSE;
  foreach ($results as $result) {
    $object = islandora_object_load($result['pid']['value']);
    $last_date = $result['date']['value'];
    $success = TRUE;
    try {
      $object->purgeDatastream('OGG');
    }
    catch (Exception $e) {
      $success = FALSE;
    }

    if ($success) {
      drush_log(dt("OGG derivative deletion succeeded for @pid.", array('@pid' => $object->id)), 'success');
    }
    else {
      drush_log(dt("OGG derivative deletion failed for @pid. Check the Drupal watchdog for detailed errors.", array('@pid' => $object->id)), 'error');
    }
  }
  if ($last_date) {
    $sandbox['last_date'] = $last_date;
  }
  $context['finished'] = empty($results);
}

/**
 * Command callback to update videos to have both TN and TN_LARGE.
 */
function drush_islandora_video_generate_tn_large() {
  batch_set(islandora_video_generate_tn_large_create_batch());
  drush_backend_batch_process();
}

function islandora_video_generate_tn_large_create_batch() {
  return array(
    'operations' => array(
      array('islandora_video_generate_tn_large_batch_operation', array()),
    ),
    'title' => t('Updating TN and TN_LARGE for videos...'),
    'init_message' => t('Preparing to update derivatives.'),
    'progress_message' => t('Time elapsed: @elapsed <br/>Estimated time remaining @estimate.'),
    'error_message' => t('An error has occurred.'),
    'file' => drupal_get_path('module', 'islandora_video') . '/islandora_video.drush.inc',
  );
}

/**
 * Constructs and performs the video batch operation.
 *
 * @param array $context
 *   The context of the Drupal batch.
 */
function islandora_video_generate_tn_large_batch_operation(&$context) {
  $video_update = 5;
  variable_set('islandora_video_use_separate_tn_large', TRUE);
  variable_set('islandora_video_make_thumbnail_locally', TRUE);
  drupal_flush_all_caches();
  // Select all objects with video Cmodel and TN but no TN_LARGE.
  $query = <<<EOQ
SELECT ?pid FROM <#ri>
WHERE {
  ?pid <fedora-model:hasModel> <info:fedora/islandora:sp_videoCModel> ;
       <fedora-view:disseminates> ?ds  .
  ?ds <fedora-view:disseminationType> <info:fedora/*/TN> .
  OPTIONAL {
    ?pid <fedora-view:disseminates> ?dslarge .
    ?dslarge <fedora-view:disseminationType> <info:fedora/*/TN_LARGE> .
  }
  FILTER (!bound(?dslarge))
}
EOQ;
  $connection = islandora_get_tuque_connection();
  $sandbox = &$context['sandbox'];
  if (!isset($sandbox['offset'])) {
    $sparql_count = $connection->repository->ri->countQuery($query, 'sparql');
    $sandbox['offset'] = 0;
    $sandbox['total'] = $sparql_count;
    if ($sandbox['total'] === 0) {
      return;
    }
  }
  $context['message'] = t('Processing results @start to @end of @total.', array(
    '@start' => $sandbox['offset'],
    '@end' => min($sandbox['offset'] + $video_update, $sandbox['total']),
    '@total' => $sandbox['total'],
  ));
  $offset_start = $sandbox['offset'];
  $query .= "
  LIMIT $video_update
  OFFSET 0
  ";
  module_load_include('inc', 'islandora', 'includes/derivatives');
  module_load_include('inc', 'islandora', 'includes/utilities');
  module_load_include('inc', 'islandora_video', 'includes/derivatives');
  $results = $connection->repository->ri->sparqlQuery($query);
  foreach ($results as $result) {
    $pid = $result['pid']['value'];
    if (islandora_namespace_accessible($pid)) {
      $object = islandora_object_load($pid);
      if (isset($object['TN'])) {
        // Save TN to a file.
        $mime_detect = new MimeDetect();
        $ext = $mime_detect->getExtension($object['TN']->mimeType);
        $file_name = str_replace(':', '-', $object->id);
        $file_uri = file_create_filename("{$file_name}TN.{$ext}", 'temporary://');
        $object['TN']->getContent($file_uri);
        $file_path = drupal_realpath($file_uri);

        if (filesize($file_path) > 0) {
          $object->purgeDatastream('TN');
          $datastream_result = islandora_video_add_datastream($object, 'TN_LARGE', $file_path, TRUE);

          if ($datastream_result['success']) {
            drush_log(dt("TN replacement succeeded for @pid.", array('@pid' => $object->id)), 'success');
          }
          // Derivatives will run to generate TN.
        }
        else {
          drush_log(dt('TN datastream content not accessible, skipping PID "@pid"...', array('@pid' => $pid)), 'warning');
        }
      }
      else {
        drush_log(dt('TN datastream not found, skipping PID "@pid"...', array('@pid' => $pid)), 'warning');
      }
    }
    else {
      drush_log(dt('Namespace not accessible, skipping PID "@pid"...', array('@pid' => $pid)), 'notice');
    }
  }
  $sandbox['offset'] += $video_update;
  $context['finished'] = $sandbox['offset'] / $sandbox['total'];
}
