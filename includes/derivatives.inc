<?php

/**
 * @file
 * Create derivatives for video content.
 */

/**
 * This function will create an MP4 file for upload.
 *
 * @param AbstractObject $object
 *   An AbstractObject representing an object within Fedora.
 * @param bool $force
 *   Whether derivative generation is being forced or not.
 *
 * @return array
 *   An array describing the outcome of the MP4 creation.
 *
 * @see hook_islandora_derivative()
 */
function islandora_video_create_mp4(AbstractObject $object, $force = FALSE) {
  if ($force || !isset($object['MP4'])) {
    $archival_file = islandora_video_copy_obj_datastream($object);
    if ($archival_file === FALSE) {
      return islandora_video_no_obj_datastream($object->id);
    }
    $archival_path = drupal_realpath($archival_file['file']->uri);
    $out_file = $archival_path . '.mp4';
    $audio_codec = variable_get('islandora_video_mp4_audio_codec', 'aac');

    $ffmpeg_executable = variable_get('islandora_video_ffmpeg_path', 'ffmpeg');
    $command = "$ffmpeg_executable -i $archival_path -f mp4 -vcodec libx264 -preset medium -acodec $audio_codec -strict -2 -ab 128k -ac 2 -async 1 -movflags faststart $out_file";
    $return_value = FALSE;
    exec($command, $output, $return_value);
    file_delete($archival_file['file']);
    if ($return_value === 0) {
      return islandora_video_add_datastream($object, 'MP4', $out_file);
    }
    else {
      return array(
        'success' => FALSE,
        'messages' => array(
          array(
            'message' => t('Ffmpeg encoding failed for object @pid - MP4 creation was skipped.'),
            'message_sub' => array('@pid' => $object->id),
            'type' => 'watchdog',
            'severity' => WATCHDOG_ERROR,
          ),
        ),
      );
    }
  }
}

/**
 * This function will create an OGG file for upload.
 *
 * @param AbstractObject $object
 *   An AbstractObject representing an object within Fedora.
 * @param bool $force
 *   Whether derivative generation is being forced or not.
 *
 * @return array
 *   An array describing the outcome of the OGG creation.
 *
 * @see hook_islandora_derivative()
 */
function islandora_video_create_ogg(AbstractObject $object, $force = FALSE) {
  if ($force || !isset($object['OGG'])) {
    $archival_file = islandora_video_copy_obj_datastream($object);
    if ($archival_file === FALSE) {
      return islandora_video_no_obj_datastream($object->id);
    }
    $archival_path = drupal_realpath($archival_file['file']->uri);
    $out_file = $archival_path . '.ogg';

    $theora_executable = variable_get('islandora_video_ffmpeg2theora_path', 'ffmpeg2theora');
    $command = "$theora_executable $archival_path -o $out_file";
    $return_value = FALSE;
    exec($command, $output, $return_value);
    file_delete($archival_file['file']);
    if ($return_value === 0) {
      return islandora_video_add_datastream($object, 'OGG', $out_file);
    }
    else {
      return array(
        'success' => FALSE,
        'messages' => array(
          array(
            'message' => t('Ffmpeg2theora encoding failed for object @pid - OGG creation was skipped.'),
            'message_sub' => array('@pid' => $object->id),
            'type' => 'watchdog',
            'severity' => WATCHDOG_ERROR,
          ),
        ),
      );
    }
  }
}

/**
 * This function will create an MKV file for upload.
 *
 * @param AbstractObject $object
 *   An AbstractObject representing an object within Fedora.
 * @param bool $force
 *   Whether derivative generation is being forced or not.
 *
 * @return array
 *   An array describing the outcome of the MKV creation.
 *
 * @see hook_islandora_derivative()
 */
function islandora_video_create_mkv(AbstractObject $object, $force = FALSE) {
  if ($force || !isset($object['MKV'])) {
    $archival_file = islandora_video_copy_obj_datastream($object);
    if ($archival_file === FALSE) {
      return islandora_video_no_obj_datastream($object->id);
    }
    $archival_path = drupal_realpath($archival_file['file']->uri);
    $out_file = $archival_path . '.mkv';

    $ffmpeg_executable = variable_get('islandora_video_ffmpeg_path', 'ffmpeg');
    $command = "$ffmpeg_executable -i $archival_path $out_file";
    $return_value = FALSE;
    exec($command, $output, $return_value);
    file_delete($archival_file['file']);
    if ($return_value === 0) {
      return islandora_video_add_datastream($object, 'MKV', $out_file);
    }
    else {
      return array(
        'success' => FALSE,
        'messages' => array(
          array(
            'message' => t('Ffmpeg encoding failed for object @pid - MKV creation was skipped.'),
            'message_sub' => array('@pid' => $object->id),
            'type' => 'watchdog',
            'severity' => WATCHDOG_ERROR,
          ),
        ),
      );
    }
  }
}

/**
 * Extract a screenshot from the video file.
 *
 * This function copies the OBJ datastream and extracts a full-sized
 * screenshot from the halfway through the video.
 *
 * @param AbstractObject $object
 *   An object in a Fedora repository
 *
 * @return string|bool
 *   The file path to the extracted file, or FALSE if didn't work.
 */
function islandora_video_extract_screenshot(AbstractObject $object) {
  $archival_file = islandora_video_copy_obj_datastream($object);
  if ($archival_file === FALSE) {
    return FALSE;
  }
  $archival_path = drupal_realpath($archival_file['file']->uri);
  $out_file = $archival_path . '-TN.jpg';
  $ffmpeg_executable = variable_get('islandora_video_ffmpeg_path', 'ffmpeg');

  $vid_length = "$ffmpeg_executable -i $archival_path 2>&1";
  exec($vid_length, $time_output, $ret_value);

  $dur_match = FALSE;
  $duration = '';
  foreach ($time_output as $key => $value) {
    preg_match('/Duration: (.*), start/', $value, $time_match);
    if (count($time_match)) {
      $dur_match = TRUE;
      $duration = $time_match[1];
      break;
    }
  }
  if ($dur_match) {
    // Snip off the ms because we don't care about them.
    $time_val = preg_replace('/\.(.*)/', '', $duration);
    $time_array = explode(':', $time_val);
    // Calculate the number of seconds halfway through the video.
    $output_time = floor((($time_array[0] * 360) + ($time_array[1] * 60) + $time_array[2]) / 2);

    $tn_creation = "$ffmpeg_executable -itsoffset -2 -ss $output_time -i $archival_path -vcodec mjpeg -vframes 1 -an -f rawvideo $out_file";

    $return_value = 1;
    exec($tn_creation, $output, $return_value);
    file_delete($archival_file['file']);
    if ($return_value === 0) {
      return ($out_file);
    }
    else {
      return FALSE;
    }
  }
  else {
    return FALSE;
  }
}

/**
 * Create the TN datastream from the OBJ datastream (no resizing).
 *
 * @param AbstractObject $object
 *   An AbstractObject representing an object within Fedora.
 * @param bool $force
 *   Whether derivative generation is being forced or not.
 *
 * @return array
 *   An array describing the outcome of the TN creation.
 *
 * @see hook_islandora_derivative()
 */
function islandora_video_create_thumbnail(AbstractObject $object, $force = FALSE) {
  if ($force || !isset($object['TN'])) {
    if (!isset($object['OBJ'])) {
      return islandora_video_no_obj_datastream($object->id);
    }
    $out_file = islandora_video_extract_screenshot($object);
    if ($out_file != FALSE) {
      // Set the returned file as TN
      return islandora_video_add_datastream($object, 'TN', $out_file);
    }
    else {
      // Unable to generate with Ffmpeg, add default TN.
      $stored_thumbnail = drupal_get_path('module', 'islandora_video') . '/images/crystal_clear_app_camera.png';
      return islandora_video_add_datastream($object, 'TN', $stored_thumbnail, FALSE);
    }
  }
}

/**
 * Create the TN_LARGE datastream from the OBJ datastream (no resizing).
 *
 * @param AbstractObject $object
 *   An AbstractObject representing an object within Fedora.
 * @param bool $force
 *   Whether derivative generation is being forced or not.
 *
 * @return array
 *   An array describing the outcome of the TN_LARGE creation.
 *
 * @see hook_islandora_derivative()
 */
function islandora_video_create_thumbnail_large(AbstractObject $object, $force = FALSE) {
  if ($force || !isset($object['TN_LARGE'])) {
    if (!isset($object['OBJ'])) {
      return islandora_video_no_obj_datastream($object->id);
    }
    $out_file = islandora_video_extract_screenshot($object);
    if ($out_file != FALSE) {
      // Set the returned file as TN
      return islandora_video_add_datastream($object, 'TN_LARGE', $out_file);
    }
    else {
      // Unable to generate with Ffmpeg, add default TN.
      $stored_thumbnail = drupal_get_path('module', 'islandora_video') . '/images/crystal_clear_app_camera.png';
      return islandora_video_add_datastream($object, 'TN_LARGE', $stored_thumbnail, FALSE);
    }
  }
}

/**
 * Create a thumbnail-sized TN datastream from TN_LARGE
 *
 * @param AbstractObject $object
 *   An object within Fedora
 * @param bool $force
 *   Whether derivative generation is being forced or not.
 *
 * @return array
 *   An array describing the outcome of the TN generation.
 *
 * @see hook_islandora_derivative()
 */
function islandora_video_resize_thumbnail(AbstractObject $object, $force = FALSE) {
  if ($force || !isset($object['TN'])) {
    if (!isset($object['TN_LARGE'])) {
      return array(
        'success' => FALSE,
        'messages' => array(
          array(
            'message' => t('Failed to generate @dsid datastream for @pid. TN_LARGE datastream not available.'),
            'message_sub' => array(
              '@dsid' => 'TN',
              '@pid' => $object->id,
            ),
            'type' => 'watchdog',
            'severity' => WATCHDOG_ERROR,
          ),
        ),
      );
    }
    $mime_detect = new MimeDetect();
    $ext = $mime_detect->getExtension($object['TN_LARGE']->mimeType);
    $file_name = str_replace(':', '-', $object->id);
    $file_uri = file_create_filename("{$file_name}TN_LARGE.{$ext}", 'temporary://');
    $object['TN_LARGE']->getContent($file_uri);
    $file_path = drupal_realpath($file_uri);

    if (module_exists('imagemagick')) {
      module_load_include('module', 'imagemagick');
      $resized_out_file = $file_path . '_200x200-TN.jpg';
      $args = array();
      $args[] = '-quality ' . escapeshellarg(variable_get('imagemagick_quality', 100));
      $args[] = '-resize ' . escapeshellarg("200 x 200");
      $command = escapeshellarg($file_path) . ' ' . implode(' ', $args) . ' ' . escapeshellarg($resized_out_file);
      $output = array();
      $ret = -1;
      if (_imagemagick_convert_exec($command, $output, $ret) !== TRUE) {
        return array(
          'success' => FALSE,
          'messages' => array(
            array(
              'message' => t('Failed to add @dsid datastream for @pid. Imagemagick failed to convert. Error: "@output" from Command: "@command"'),
              'message_sub' => array(
                '@dsid' => 'TN',
                '@pid' => $object->id,
                '@output' => implode('<br/>', $output),
                '@command' => $command,
              ),
              'type' => 'watchdog',
              'severity' => WATCHDOG_ERROR,
            ),
          ),
        );
      }
      file_unmanaged_delete($file_path);
      return islandora_video_add_datastream($object, 'TN', $resized_out_file);
    }
    else {
      return islandora_video_add_datastream($object, 'TN', $file_path);
    }
  }
}

/**
 * Adds the file as a datastream to the given object.
 *
 * @param AbstractObject $object
 *   The object to add the datastream to.
 * @param string $datastream_id
 *   The datastream id of the added datastream.
 * @param string $file_uri
 *   A URI to the file containing the content for the datastream.
 * @param bool $delete_file
 *   A flag indicating if the file should be deleted after ingest.
 *
 * @return array
 *   An array describing the outcome of the datastream creation.
 *
 * @see hook_islandora_derivative()
 */
function islandora_video_add_datastream($object, $datastream_id, $file_uri, $delete_file = TRUE) {
  try {
    $ingest = !isset($object[$datastream_id]);
    $mime_detector = new MimeDetect();

    if ($ingest) {
      $ds = $object->constructDatastream($datastream_id, "M");
      $ds->label = $datastream_id;
    }
    else {
      $ds = $object[$datastream_id];
    }
    if ($ds->mimetype != $mime_detector->getMimetype($file_uri)) {
      $ds->mimetype = $mime_detector->getMimetype($file_uri);
    }
    $ds->setContentFromFile($file_uri);
    if ($ingest) {
      $object->ingestDatastream($ds);
    }
    if ($delete_file && file_exists($file_uri)) {
      file_unmanaged_delete($file_uri);
    }
    return array(
      'success' => TRUE,
      'messages' => array(
        array(
          'message' => t('Created @dsid derivative for OBJ (@pid).'),
          'message_sub' => array(
            '@dsid' => $datastream_id,
            '@pid' => $object->id,
          ),
          'type' => 'dsm',
        ),
      ),
    );
  }
  catch (exception $e) {
    return array(
      'success' => FALSE,
      'messages' => array(
        array(
          'message' => t('Failed to add @dsid datastream for @pid. Error message: "@message" with trace: @stack'),
          'message_sub' => array(
            '@dsid' => $datastream_id,
            '@pid' => $object->id,
            '@message' => $e->getMessage(),
            '@stack' => $e->getTraceAsString(),
          ),
          'type' => 'watchdog',
          'severity' => WATCHDOG_ERROR,
        ),
      ),
    );
  }
}

/**
 * Copies the OBJ to the file system.
 *
 * @param AbstractObject $object
 *   An AbstractObject representing an object within Fedora.
 *
 * @return array|bool
 *   FALSE when the object doesn't have the OBJ, an array otherwise.
 */
function islandora_video_copy_obj_datastream(AbstractObject $object) {
  $mime_detect = new MimeDetect();
  if (!isset($object['OBJ'])) {
    return FALSE;
  }
  $ext = $mime_detect->getExtension($object['OBJ']->mimeType);
  $file_name = str_replace(':', '-', $object->id);

  // Create a file object we can save.
  $file_uri = file_create_filename("{$file_name}OBJ.{$ext}", 'temporary://');
  $file = new stdClass();
  $file->uri = $file_uri;
  $file->filename = $file_name;
  $file->filemime = $object['OBJ']->mimeType;
  // Temporary.
  $file->status = 0;
  $object['OBJ']->getContent($file_uri);
  $original_file = file_save($file);
  return array(
    'file' => $original_file,
    'extension' => $ext,
    'filename' => $file_name,
  );
}

/**
 * Helper function that returns a message when no OBJ datastream exists.
 *
 * @param string $pid
 *   The PID of the object that derivatives are being generated on.
 *
 * @return array
 *   An array describing the failure of the operation.
 *
 * @see hook_islandora_derivative()
 */
function islandora_video_no_obj_datastream($pid) {
  return array(
    'success' => FALSE,
    'messages' => array(
      array(
        'message' => t('Could not create video derivatives for %s. OBJ datastream does not exist.'),
        'message_sub' => array('%s' => $pid),
        'type' => 'watchdog',
        'severity' => WATCHDOG_ERROR,
      ),
    ),
  );
}
