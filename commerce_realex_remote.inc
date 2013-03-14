<?php

/**
 * @file
 * Callback methods for Realex Remote integration.
 */

/**
 * Implements hook_commerce_payment_method_settings_form().
 */
function commerce_realex_remote_commerce_payment_method_settings_form($settings = NULL) {
  $form = array();

  $form['merchant_id'] = array(
    '#type' => 'textfield',
    '#title' => t('Realex Merchant ID'),
    '#default_value' => isset($settings['merchant_id']) ? $settings['merchant_id'] : '',
    '#size' => 16,
    '#required' => TRUE,
  );

  $form['account_id'] = array(
    '#type' => 'textfield',
    '#title' => t('Realex Account ID'),
    '#default_value' => isset($settings['account_id']) ? $settings['account_id'] : '',
    '#size' => 16,
    '#required' => FALSE,
  );

  $form['shared_secret'] = array(
    '#type' => 'textfield',
    '#title' => t('Shared secret'),
    '#default_value' => isset($settings['shared_secret']) ?  $settings['shared_secret'] : '',
    '#size' => 16,
    '#required' => TRUE,
  );

  $form['remote_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Realex Remote XML URL'),
    '#description' => t('The Realex URL to submit the order to.'),
    '#default_value' => isset($settings['remote_url']) ?  $settings['remote_url'] : 'https://epage.payandshop.com/epage-remote.cgi',
    '#size' => 50,
    '#required' => TRUE,
  );

  if (module_exists('commerce_3d_secure')) {
    $form['3dsecure'] = array(
      '#type' => 'checkbox',
      '#title' => t('Use 3D Secure'),
      '#default_value' => isset($settings['3dsecure']) ? $settings['3dsecure'] : 0,
    );

    $form['3dsecure_remote_url'] = array(
      '#type' => 'textfield',
      '#title' => t('Realex 3D Secure XML URL'),
      '#description' => t('The Realex URL to submit the order to.'),
      '#default_value' => isset($settings['3dsecure_remote_url']) ? $settings['3dsecure_remote_url'] : 'https://epage.payandshop.com/epage-3dsecure.cgi',
      '#size' => 50,
      '#required' => TRUE,
      '#states' => array(
        'visible' => array(
          ':input[name="parameter[payment_method][settings][payment_method][settings][3dsecure]"]' => array('checked' => TRUE),
        ),
      ),
    );
  }
  else {
    $form['3dsecure'] = array(
      '#type' => 'checkbox',
      '#title' => t('Use 3D Secure'),
      '#default_value' => 0,
      '#attributes' => array('disabled' => 'disabled'),
      '#description' => t('Enable the Commerce 3D Secure module if you wish to enable 3D Secure transactions for Realex.'),
    );
  }

  $form['auto_settle'] = array(
    '#type' => 'select',
    '#title' => t('Auto settle flag'),
    '#options' => array(1 => t('Enabled'), 0 => t('Disabled')),
    '#default_value' => isset($settings['auto_settle']) ?  $settings['auto_settle'] : 1,
    '#required' => TRUE,
  );

  $form['varref'] = array(
    '#type' => 'textfield',
    '#title' => t('Variable reference'),
    '#description' => t('Any reference you also would like to assign to the customer. This can allow checking, using realscore, of previous transactions by this customer.'),
    '#default_value' => isset($settings['varref']) ?  $settings['varref'] : '',
    '#size' => 50,
    '#required' => FALSE,
  );


  /*
  $form['dcc_provider'] = array(
    '#type' => 'select',
    '#title' => t('Direct Currency Conversion (DCC)'),
    '#options' => array(
      'none' => 'None',
      //'fintrax' => 'Fintrax', Not available yet
      'fexco' => 'Fexco',
      'euroConex' => 'Elavon',
     ),
    '#description' => t('Should DCC be available to the user? You must have this preconfigured on your Realex sub-account in advance.'),
    '#default_value' => isset($settings['dcc_provider']) ? $settings['dcc_provider'] : 'none',
  );
  */

  $cctypes = _commerce_realex_supported_card_types();
  $form['card_types'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Available card types'),
    '#options' => $cctypes,
    '#default_value' => isset($settings['card_types']) ? $settings['card_types'] : array(),
    '#required' => TRUE,
  );

  $form['cctype_subaccounts_enabled'] = array(
    '#type' => 'checkbox',
    '#title' => t('Use separate accounts for each card type'),
    '#description' => t('Some configurations require different accounts to be used for different card types.'),
    '#default_value' => isset($settings['cctype_subaccounts_enabled']) ? $settings['cctype_subaccounts_enabled'] : array(),
  );

  $form['cctype_accounts'] = array(
    '#type' => 'fieldset',
    '#title' => t('Realex card type account settings'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#states' => array(
      'visible' => array(
        ':input[name="parameter[payment_method][settings][payment_method][settings][cctype_subaccounts_enabled]"]' => array('checked' => TRUE),
      ),
    ),
  );
  foreach ($cctypes as $cctype => $label) {
    $form['cctype_accounts'][$cctype] = array(
      '#type' => 'textfield',
      '#title' => t('@cctype account', array('@cctype' => $label)),
      '#default_value' => !empty($settings['cctype_accounts'][$cctype]) ? $settings['cctype_accounts'][$cctype] : '',
      '#required' => FALSE,
    );
  }

  return $form;
}

/**
 * Implements hook_commerce_payment_method_submit_form().
 *
 * Set allowed credit card types.
 */
function commerce_realex_remote_commerce_payment_method_submit_form($payment_method, $pane_values, $checkout_pane, $order) {
  module_load_include('inc', 'commerce_payment', 'includes/commerce_payment.credit_card');
  $supported_card_types = _commerce_realex_supported_card_types();
  $available_card_types = $payment_method['settings']['card_types'];
  $card_types = array_intersect(array_keys($supported_card_types), $available_card_types);

  // Set which cards are available to the user.
  $credit_card_settings = array(
    'owner' => '',
    'type' => $card_types,
    'number' => '',
    'start_date' => NULL,
    'issue' => '',
    'code' => '',
  );

  // Get credit card fields.
  $form = commerce_payment_credit_card_form($credit_card_settings);

  return $form;
}

/**
 * Implements hook_commerce_payment_method_submit_form_validate().
 *
 * Validate the credit card fields.
 */
function commerce_realex_remote_commerce_payment_method_submit_form_validate($payment_method, $pane_form, $pane_values, $order, $form_parents = array()) {
  module_load_include('inc', 'commerce_payment', 'includes/commerce_payment.credit_card');

  $settings = array(
    'form_parents' => array_merge($form_parents, array('credit_card')),
  );
  $prefix = implode('][', $settings['form_parents']) . '][';

  $card_type = $pane_values['credit_card']['type'];
  if ($card_type == 'switch') {
    if (empty($pane_values['credit_card']['issue'])) {
      form_set_error($prefix . 'issue', t('Issue number field is required.'));
      return FALSE;
    }
  }
  elseif ($card_type != 'laser') {
    if (empty($pane_values['credit_card']['code'])) {
      form_set_error($prefix . 'code', t('Security code field is required.'));
      return FALSE;
    }
  }
  if (!commerce_payment_credit_card_validate($pane_values['credit_card'], $settings)) {
    return FALSE;
  }
}

/**
 * Implements hook_commerce_payment_method_submit_form_submit().
 */
function commerce_realex_remote_commerce_payment_method_submit_form_submit($payment_method, $pane_form, $pane_values, &$order, $charge) {
  $start_date = NULL;
  if (!empty($pane_values['credit_card']['start_month'])) {
    $start_date = sprintf("%02d%02d", $pane_values['credit_card']['start_month'], substr($pane_values['credit_card']['start_year'], -2));
  }
  $expiry_date = sprintf("%02d%02d", $pane_values['credit_card']['exp_month'], substr($pane_values['credit_card']['exp_year'], -2));

  $request = array();
  $request['timestamp'] = strftime('%Y%m%d%H%M%S');
  mt_srand((double) microtime() * 1000000);

  $request['order_number'] = $order->order_number;
  $request['orderid'] = $request['order_number'] . $request['timestamp'];
  $request['merchantid'] = check_plain($payment_method['settings']['merchant_id']);
  $request['account'] = check_plain($payment_method['settings']['account_id']);
  $request['secret'] = check_plain($payment_method['settings']['shared_secret']);
  $request['auto_settle'] = $payment_method['settings']['auto_settle'];
  if (!empty($payment_method['settings']['varref'])) {
    $request['varref'] = $payment_method['settings']['varref'];
  }
  // Allow other modules to alter the variable reference - used to tag orders.
  drupal_alter('commerce_realex_varref', $request['varref'], $order);
  $request['varref'] = check_plain($request['varref']);

  $request['3dsecure'] = (module_exists('commerce_3d_secure') ?  $payment_method['settings']['3dsecure']: 0);

  //$request['dcc_provider'] = $payment_method['settings']['dcc_provider'];

  $request['chname'] = check_plain($pane_values['credit_card']['owner']);
  $request['cctype'] = _commerce_realex_get_card_type($pane_values['credit_card']['type']);
  $request['ccnumber'] = check_plain($pane_values['credit_card']['number']);
  $request['cvn'] = isset($pane_values['credit_card']['code']) ? check_plain($pane_values['credit_card']['code']) : NULL;
  $request['issueno'] = isset($pane_values['credit_card']['issue']) ? check_plain($pane_values['credit_card']['issue']) : '';
  $request['start_date'] = check_plain($start_date);
  $request['expiry_date'] = check_plain($expiry_date);

  // Sub-accounts can differ by card type
  if ($payment_method['settings']['cctype_subaccounts_enabled']) {
    $subaccount = $payment_method['settings']['cctype_accounts'][$pane_values['credit_card']['type']];
    $request['account'] = !empty($subaccount) ? check_plain($subaccount) : $request['account'];
  }

  // Load the customer profile for the order.
  if (!empty($order->commerce_customer_billing)) {
    $order_wrapper = entity_metadata_wrapper('commerce_order', $order);
    $profile = $order_wrapper->commerce_customer_billing->value();
    $profile_wrapper = entity_metadata_wrapper('commerce_customer_profile', $profile);
    $address = $profile_wrapper->commerce_customer_address->value();
    $request['customer_id'] = $profile->profile_id;
    if (isset($address['name_line'])) {
      $request['customer_name'] = check_plain($address['name_line']);
    }
    elseif (isset($address['first_name'])) {
      $request['customer_name'] = check_plain($address['first_name'] . ' ' .  $address['last_name']);
    }
    $request['street_one'] = isset($address['thoroughfare']) ? check_plain($address['thoroughfare']) : '';
    $request['postcode'] = isset($address['postal_code']) ? check_plain($address['postal_code']) : '';
    $request['city'] = isset($address['locality']) ? check_plain($address['locality']) : '';
    $request['region'] = isset($address['administrative_area']) ? check_plain($address['administrative_area']) : '';
    $request['country'] = isset($address['country']) ? check_plain($address['country']) : '';
  }
  $request['amount'] = $charge['amount'];
  $request['currency'] = $charge['currency_code'];
  $request['ip_address'] = ip_address();

  // Get products purchased.
  $products = array();
  $comments = array();
  $order_wrapper = entity_metadata_wrapper('commerce_order', $order);
  foreach ($order_wrapper->commerce_line_items as $delta => $line_item_wrapper) {
    $line_item = $line_item_wrapper->value();
    // If the line item is a product line item...
    if (in_array($line_item->type, commerce_product_line_item_types())) {
      $product = $line_item_wrapper->commerce_product->value();
      $products[] = $product->product_id;
      $comments[] = sprintf("%dx %s",$line_item->quantity, check_plain($product->title));
    }
  }

  $request['products'] = implode(',', $products);
  $request['comments'][0] = t('Products purchased:') . ' ' . implode(', ', $comments);

  // Build sha1 hash.
  $request['sha1hash'] = _commerce_realex_build_request_sha1hash($request);

  // 3D Secure transaction.
  if ($request['3dsecure'] && module_exists('commerce_3d_secure')) {
    commerce_realex_remote_do_3dsecure_verify_enrolled_request($payment_method, $request, $order);
  }

  // DCC enabled, so get offer rather than processing payment.
  //elseif ($request['dcc_provider'] != 'none') {
   // $dcc_message = commerce_realex_remote_get_dcc_offer($payment_method, $pane_values, $order, $charge);
  //}

  // Standard authorisation.
  else {
    commerce_realex_remote_do_auth_request($payment_method, $request, $order);
  }

  return;
}

/**
 * Do a payment authorization request.
 */
function commerce_realex_remote_do_auth_request($payment_method, $request, &$order) {
  // Send the request.
  $type = 'auth';
  $url = $payment_method['settings']['remote_url'];
  $xml = _commerce_realex_remote_build_xml_request($request, $type);
  $response = _commerce_realex_send_request($url, $xml);

  // Verify response.
  $success = _commerce_realex_remote_verify_response($type, $request, $response, $order, $payment_method);

  // Successful payment.
  if ($success) {
    // Save the transaction.
    $transaction = commerce_realex_remote_transaction_save($payment_method, $order, $response);
    //$order->data['commerce_realex_remote'] = $pane_values;
    $order->data['commerce_realex_remote']['transaction'] = $transaction;
    $redirect_url = 'commerce-realex/complete/' . $order->order_number;
    commerce_3d_secure_clear_iframe($redirect_url);
  }

  // Unsuccessful payments.
  else {
    // Payment failure.
    if (!isset($order->data['extra_authorisation'])) {
      drupal_set_message(t('We were unable to process your credit card payment; reason provided by bank: %message. If the problem persists, contact us to complete your order.', array('%message' => $response['message'])), 'warning');
      watchdog('commerce_realex', 'Payment failed: %message', array('%message' => $response['message']), WATCHDOG_WARNING);
      drupal_goto('checkout');
      if (empty($_SESSION['realex_failures'])) {
        $_SESSION['realex_failures'] = 0;
      }
      $_SESSION['realex_failures']++;
    }
    // 3D secure failure.
    elseif (isset($request['cavv'])) {
      $redirect = 'commerce-realex/payment/' . $order->order_number;
      commerce_3d_secure_clear_iframe($redirect);
    }
  }
}


/**
 * Do a 3D Secure verify enrolled request.
 */
function commerce_realex_remote_do_3dsecure_verify_enrolled_request($payment_method, $request, &$order) {
  // Send the request.
  $type = '3ds-verifyenrolled';
  $url = $payment_method['settings']['3dsecure_remote_url'];
  $xml = _commerce_realex_remote_build_xml_request($request, $type);
  $response = _commerce_realex_send_request($url, $xml);

  // Verify response.
  $success = _commerce_realex_remote_verify_response($type, $request, $response, $order, $payment_method);

  // Successful payment.
  if ($success) {
    // Create a $md variable which is a string of useful information that will be mirrored in the 3d process.
    $md = 'payment_method_id=' . $payment_method['method_id'] . '&orderid=' . $response['orderid'] . '&ccnumber=' . $request['ccnumber'] . '&chname=' . $request['chname'] . '&cctype=' . $request['cctype'] . '&issueno=' . $request['issueno'] . '&currency=' . $request['currency'] . '&amount=' . $request['amount'] . '&expiry_date=' . $request['expiry_date'];
    // @TODO sort encryption
    // $key = 'realex' . $response['pareq'] . 'commerce';
    // $md = base64_encode(mcrypt_encrypt(MCRYPT_RIJNDAEL_256, md5($key), $md, MCRYPT_MODE_CBC, md5(md5($key))));

    // Save the data to process the 3D Secure response.
    $secure_data = array();
    $secure_data['PAReq'] = $response['pareq'];
    $secure_data['ACSURL'] = $response['url']; // URL to use.
    $secure_data['MD'] = $md; // Merchant data.
    // The URL that the ACS should reply to.
    $secure_data['TermUrl'] = url('commerce-realex/3d-secure-callback/' . $order->order_id, array('absolute' => TRUE));
    // Add the 3D Secure info to the order object.
    $order->data['extra_authorisation'] = $secure_data;
  }

  // Payment failure.
  else {
    // Cardholder is not enrolled.
    if ($response['result'] == 110 && $response['enrolled'] == 'N' && empty($response['url'])) {
      if ($response['cctype'] == 'VISA') {
        $request['eci'] = 6;
      }
      elseif ($response['cctype'] == 'MC' || $response['cctype'] == 'SWITCH') {
        $request['eci'] = 1;
      }
      commerce_realex_remote_do_auth_request($payment_method, $request, $order);
      watchdog('commerce_realex', 'Customer is not 3d enrolled, liability is shifted so transaction will be completed (orderid: @order_id)', array('@order_id' => $response['orderid']));

    }
    // Other response codes.
    else {
      drupal_set_message(t('We were unable to process your credit card payment as we were unable to ascertain whether the card is 3D Secure; reason provided by bank: %message. If the problem persists, contact us to complete your order.', array('%message' => $response['message'])), 'warning');
      watchdog('commerce_realex', 'Payment failed: %message', array('%message' => $response['message']), WATCHDOG_WARNING);
      if (empty($_SESSION['realex_failures'])) {
        $_SESSION['realex_failures'] = 0;
      }
      $_SESSION['realex_failures']++;
      drupal_goto('checkout');
    }
  }
}

/**
 * Callback function for 3D secure.
 *
 * @param order
 *    The Commerce Order object.
 */
function commerce_realex_3dsecure_callback($order) {
  // Validate response.
  if (!isset($_POST['MD']) || !isset($_POST['PaRes'])) {
    watchdog('commerce_realex', 'Invalid data received in 3D Secure response', array(), WATCHDOG_ERROR);
    return drupal_not_found();
  }

  $md = $_POST['MD'];
  // @TODO -  add in decryption
  // $key = 'realex' . $order->data['extra_authorisation']['PAReq'] . 'commerce';
  // $md = rtrim(mcrypt_decrypt(MCRYPT_RIJNDAEL_256, md5($key), base64_decode($md), MCRYPT_MODE_CBC, md5(md5($key))), "\0");

  // The MD POST variable makes the foundation for our $request for 3ds-verifysig.
  $md = explode('&', $md);
  foreach ($md as $m) {
    list($field, $content) = explode('=', $m);
    $request[$field] = $content;
  }

  $method_id = $request['payment_method_id'];
  $payment_method = commerce_payment_method_load($method_id);
  if (empty($order->payment_methods)) {
    $order->payment_methods = array();
    rules_invoke_all('commerce_payment_methods', $order);
  }
  $payment_method['settings'] = $order->payment_methods[$method_id . '|commerce_payment_' . $method_id]['settings'];

  // Update request array.
  $request['pares'] = $_POST['PaRes'];
  $request['timestamp'] = strftime('%Y%m%d%H%M%S');
  $request['merchantid'] = check_plain($payment_method['settings']['merchant_id']);
  $request['account'] = check_plain($payment_method['settings']['account_id']);
  $request['secret'] = check_plain($payment_method['settings']['shared_secret']);
  $request['auto_settle'] = $payment_method['settings']['auto_settle'];
  if (!empty($payment_method['settings']['varref'])) {
    $request['varref'] = $payment_method['settings']['varref'];
  }
  // Allow other modules to alter the variable reference - used to tag orders.
  drupal_alter('commerce_realex_varref', $request['varref'], $order);
  $request['varref'] = check_plain($request['varref']);
  mt_srand((double)microtime() * 1000000);

  // Get products purchased.
  $products = array();
  $comments = array();
  $order_wrapper = entity_metadata_wrapper('commerce_order', $order);
  foreach ($order_wrapper->commerce_line_items as $delta => $line_item_wrapper) {
    $line_item = $line_item_wrapper->value();
    // If the line item is a product line item...
    if (in_array($line_item->type, commerce_product_line_item_types())) {
      $product = $line_item_wrapper->commerce_product->value();
      $products[] = $product->product_id;
      $comments[] = sprintf("%dx %s",$line_item->quantity, check_plain($product->title));
    }
  }

  $request['products'] = implode(',', $products);
  $request['comments'][0] = t('Products purchased:') . ' ' . implode(', ', $comments);

  // Build the sha1hash.
  $request['sha1hash'] = _commerce_realex_build_request_sha1hash($request, '3ds-verifysig');

  // Rebuild the xml to check the signature.
  $xml = _commerce_realex_remote_build_xml_request($request, '3ds-verifysig');

  // Send the request.
  $url = $payment_method['settings']['3dsecure_remote_url'];
  $response = _commerce_realex_send_request($url, $xml);

  // Add the mpi fields to the request array.
  if (isset($response['threedsecure_eci'])) {
    $request['3dsecure'] = TRUE;
    $request['eci'] = $response['threedsecure_eci'];
    $request['cavv'] = $response['threedsecure_cavv'];
    $request['xid'] = $response['threedsecure_xid'];
  }
  _commerce_realex_process_3dsecure_verifysig_response($response, $request, $order);
}

/**
 * Process 3d signature response.
 *
 * @param response
 *    An array of the response information.
 * @param request
 *    An array with all the transaction information.
 * @param order
 *    Commerce order object.
 */
function _commerce_realex_process_3dsecure_verifysig_response($response, $request, $order) {
  $auth_sent = FALSE;

  if (!isset($response['result'])) {
    watchdog('commerce_realex', 'No results code, something has gone wrong with the transaction.', array(), WATCHDOG_ERROR);
    drupal_goto('checkout/' . $order->order_number . '/payment');
  }

  $method_id = $request['payment_method_id'];
  $payment_method = commerce_payment_method_load($method_id);
  if (empty($order->payment_methods)) {
    $order->payment_methods = array();
    rules_invoke_all('commerce_payment_methods', $order);
  }
  $payment_method['settings'] = $order->payment_methods[$method_id . '|commerce_payment_' . $method_id]['settings'];

  $success = _commerce_realex_remote_verify_response('3ds-verifysig', $request, &$response, $order, $payment_method);

  // Successful response.
  if ($success) {
    switch ($response['threedsecure_status']) {
      case 'Y':
        // Cardholder successfully authenticated.
        commerce_realex_remote_do_auth_request($payment_method, $request, $order);
        $auth_sent = TRUE;
        break;

      case 'A':
        // The cardholder is enrolled and the bank acknowledges the attempted
        // authentication. Typically 3D secure process failed at the bank and so
        // liability has been shifted to allow the transaction to continue.
        commerce_realex_remote_do_auth_request($payment_method, $request, $order);
        watchdog('commerce_realex', 'An attempt has been acknowledged for 3d authentication, liability is shifted so transaction will be completed', array('@order_id' => $response['orderid']));
        break;

      case 'N':
        // The cardholder did not authenticate successfully – if you authorise
        // this transaction you will be liable for any chargebacks.
      case 'U':
        // Cardholder authentication temporarily unavailable – no liability
        // shift available.
      default:
        break;
    }
  }

  if (!$auth_sent) {
    // No authentication sent, inform customer and client what has gone wrong.
    $message = $response['message'];
    watchdog('commerce_realex', 'Payment failed: %message', array('%message' => $message), WATCHDOG_WARNING);
    drupal_set_message(t('3D Secure verification failed. Payment could not be processed.', array('%message' => $message)), 'error');

    // Get rid of the iframe and redirect to payment failed url.
    $redirect = 'commerce-realex/payment/' . $order->order_id;
    commerce_3d_secure_clear_iframe($redirect);
  }
}

/**
 * Contact Realex to get a DCC offer.
 */
function commerce_realex_remote_get_dcc_offer($payment_method, $pane_values, &$order, $charge) {
  $start_date = NULL;
  if (!empty($pane_values['credit_card']['start_month'])) {
    $start_date = sprintf("%02d%02d", $pane_values['credit_card']['start_month'], substr($pane_values['credit_card']['start_year'], -2));
  }
  $expiry_date = sprintf("%02d%02d", $pane_values['credit_card']['exp_month'], substr($pane_values['credit_card']['exp_year'], -2));

  $request = array();
  $request['timestamp'] = strftime('%Y%m%d%H%M%S');
  mt_srand((double) microtime() * 1000000);

  // TODO - review.
  // You can use any alphanumeric combination for the orderid.
  // Although each transaction must have a unique orderid.
/*
  $orderid = $order->order_id;
  if (isset($_SESSION['realex_failures'])) {
    $orderid = $orderid . '-' . $_SESSION['realex_failures'];
  }
*/

  $request['order_number'] = $order->order_number;
  $request['orderid'] = $request['order_number'] . $request['timestamp'];
  $request['merchantid'] = check_plain($payment_method['settings']['merchant_id']);
  $request['account'] = check_plain($payment_method['settings']['account_id']);
  $request['secret'] = check_plain($payment_method['settings']['shared_secret']);
  $request['auto_settle'] = $payment_method['settings']['auto_settle'];
  if (!empty($payment_method['settings']['varref'])) {
    $request['varref'] = $payment_method['settings']['varref'];
  }
  // Allow other modules to alter the variable reference - used to tag orders.
  drupal_alter('commerce_realex_varref', $request['varref'], $order);
  $request['varref'] = check_plain($request['varref']);

  $request['dcc_provider'] = check_plain($payment_method['settings']['dcc_provider']);

  $request['chname'] = check_plain($pane_values['credit_card']['owner']);
  $request['cctype'] = _commerce_realex_get_card_type($pane_values['credit_card']['type']);
  $request['ccnumber'] = check_plain($pane_values['credit_card']['number']);
  $request['cvn'] = isset($pane_values['credit_card']['code']) ? check_plain($pane_values['credit_card']['code']) : NULL;
  $request['issueno'] = isset($pane_values['credit_card']['issue']) ? check_plain($pane_values['credit_card']['issue']) : '';
  $request['start_date'] = check_plain($start_date);
  $request['expiry_date'] = check_plain($expiry_date);
  $request['amount'] = $charge['amount'];
  $request['currency'] = $charge['currency_code'];

  // Build sha1 hash.
  $request['sha1hash'] = _commerce_realex_build_request_sha1hash($request);

  // Send the request.
  $url = $payment_method['settings']['remote_url'];
  $type = 'dccrate';
  $xml = _commerce_realex_remote_build_xml_request($request, $type);
  $response = _commerce_realex_send_request($url, $xml);

  // Verify response.
  $success = _commerce_realex_remote_verify_response($type, $request, $response, $order, $payment_method);

  // Got DCC offer, set up variables.
  if ($success) {
    $cardholdercurrency = $response['cardholdercurrency'];
    $cardholderamount = $response['cardholderamount'];
    $cardholderamountdisplay = $cardholderamount / 100;
    $cardholderrate = $response['cardholderrate'];
    $merchantcurrency = $response['merchantcurrency'];
    $merchantamount = $response['merchantamount'];

    // Stash these in the session for later use.
    $_SESSION['cardholdercurrency'] = $cardholdercurrency;
    $_SESSION['cardholderamount'] = $cardholderamount;
    $_SESSION['cardholderrate'] = $cardholderrate;
    $_SESSION['has_dcc_offer'] = TRUE;

    // Save them to the order too.
    $order->data['cardholdercurrency'] = $cardholdercurrency;
    $order->data['cardholderamount'] = $cardholderamount;
    $order->data['cardholderrate'] = $cardholderrate;
    $order->data['has_dcc_offer'] = TRUE;

    $currency_names = array(
      'AUD' => t('Austalian Dollars'),
      'GBP' => t('Pounds Sterling'),
      'USD' => t('US Dollars'),
      'CHF' => t('Swiss Francs'),
      'EUR' => t('Euro'),
    );

    // Use human friendly names if we have them.
    if ($currency_names[$cardholdercurrency]) {
      $cardholdercurrency = $currency_names[$cardholdercurrency];
    }
    if ($currency_names[$merchantcurrency]) {
      $merchantcurrency = $currency_names[$merchantcurrency];
    }

    $message = t('We notice that your credit card is in @currency.', array('@currency' => $cardholdercurrency)) . '<br />';
    $message .= t('For your convenience you can opt to have this transaction charged as <br />@cardholderamountdisplay @cardholdercurrency (at an exchange rate of 1.00 @merchantcurrency = @cardholderrate @cardholdercurrency)', array(
      '@cardholderamountdisplay' => $cardholderamountdisplay,
      '@cardholdercurrency' => $cardholdercurrency,
      '@merchantcurrency' => $merchantcurrency,
      '@cardholderrate' => $cardholderrate,
      '@cardholdercurrency' => $cardholdercurrency,
    ));
    $order->data['commerce_realex_dcc_message'] = $message;
  }

  else {
    watchdog('commerce_realex', 'DCC offer failed: %message', array('%message' => $response['message']), WATCHDOG_WARNING);
  }

  return $success;
}

/**
 * Process the Realex response.
 *
 * @param $type
 *   Message response type.
 * @param $response
 *   Parsed response as an array.
 * @param $request
 *   Transaction details array.
 * @param $order
 *   Commerce order object.
 * @param $payment_method
 *   Payment method array.
 */
function _commerce_realex_remote_verify_response($type, $request, &$response, $order, $payment_method) {
  if (!isset($response['result'])) {
    watchdog('commerce_realex', 'No results code, something has gone wrong with the transaction.', array(), WATCHDOG_ERROR);
    drupal_goto('checkout/' . $order->order_id . '/payment');
  }

  $merchantid = $request['merchantid'];
  $secret = $request['secret'];
  $orderid = $request['orderid'];
  $timestamp = $response['timestamp'];
  $resultcode = $response['result'];
  $message = $response['message'];
  if ($response['result'] != '00') {
    return FALSE;
  }
  $pasref = isset($response['pasref']) ? $response['pasref'] : '';
  $authcode = isset($response['authcode']) ? $response['authcode'] : ''; // Needed for Fexco DCC.

  // This digital signature should correspond to the one Realex Payments returns
  // and can therefore be used to verify the message Realex sends back.
  $tmp = "$timestamp.$merchantid.$orderid.$resultcode.$message.$pasref.$authcode";
  $sha1hash = sha1($tmp);
  $tmp = "$sha1hash.$secret";
  $sha1hash = sha1($tmp);

  $success = FALSE;

  // Check to see if hashes match or not
  if ($sha1hash != $response['sha1hash']) {
    watchdog('commerce_realex', 'Hash match failed for order @order_id.', array('@order_id' => $response['orderid']));
    drupal_set_message(t("Hashes don't match - response not authenticated!"), 'error');
    $response['user_message'] = 'Bank connection error, please try again later';
  }
  else {
    $success = TRUE;
  }

  // Unlikely, but just in case they can ever decrypt it to get card number.
  unset($response['sha1hash']);

  return $success;
}

/**
 * Generate xml
 *
 * @param request
 *     An array with all the transaction data.
 */
function _commerce_realex_remote_build_xml_request($request, $type) {
  switch ($type) {
    case 'dccrate':
      return _commerce_realex_remote_build_xml_request_dccrate($request);
    case '3ds-verifysig':
      return _commerce_realex_remote_build_xml_request_3dsecure_verifysig($request);
    case '3ds-verifyenrolled':
      return _commerce_realex_remote_build_xml_request_3dsecure_verifyenrolled($request);
    case 'auth':
    default:
      return _commerce_realex_remote_build_xml_request_auth($request);
  }
}

/**
 * Generate an "auth" xml request.
 */
function _commerce_realex_remote_build_xml_request_auth($request) {
  $xml = "<request type='auth' timestamp='" . $request['timestamp'] . "'>
    <merchantid>" . $request['merchantid'] . "</merchantid>
    <account>" . $request['account'] . "</account>
    <orderid>" . $request['orderid'] . "</orderid>
    <amount currency='" . $request['currency'] . "'>" . $request['amount'] . "</amount>
    <card>
      <number>" . $request['ccnumber'] . "</number>
      <expdate>" . $request['expiry_date'] . "</expdate>
      <type>" . $request['cctype'] . "</type>
      <chname>" . $request['chname'] . "</chname>\n";
  if (isset($request['issueno'])) {
    $xml .= "
      <issueno>" . $request['issueno'] . "</issueno>\n";
  }
  $xml .= "</card>\n";
  if (isset($request['cvn'])) {
    $xml .="
      <cvn>
        <number>" . $request['cvn'] . "</number>
        <presind>1</presind>
      </cvn>";
  }
  if (isset($request['dcc'])) {
    $xml .="
      <dccinfo>
        <ccp>" . $request['dcc_provider'] . "</ccp>
        <type>1</type>
        <rate>" . $request['cardholderrate'] . "</rate>
        <ratetype>S</ratetype>
        <amount currency='" . $request['cardholdercurrency'] . "'>" . $request['cardholderamount'] . "</amount>
      </dccinfo>";
  }
  if (!empty($request['comments']) && is_array($request['comments'])) {
    $xml .= "
      <comments>
        <comment id='1'>" . $request['comments'][0] . "</comment>";
    if (count($request['comments']) > 1) {
      $xml .= "<comment id='2'>" . $request['comments'][1] . "</comment>";
    }
    $xml .= "
      </comments>";
  }
  if (!empty($request['3dsecure']) && module_exists('commerce_3d_secure')) {
    $xml .="<mpi>";
    if (isset($request['cavv'])) {
      $xml .= "<cavv>" . $request['cavv'] . "</cavv>";
    }
    if (isset($request['xid'])) {
      $xml .= "<xid>" . $request['xid'] . "</xid>";
    }
    if (isset($request['eci'])) {
      $xml .= "<eci>" . $request['eci'] . "</eci>";
    }
    $xml .="</mpi>";
  }
  $xml .= "
    <autosettle flag='" . $request['auto_settle'] . "'/>
    <sha1hash>" . $request['sha1hash'] . "</sha1hash>
    <tssinfo>";
  if (!empty($request['customer_id'])) {
    $xml .= "<address type=\"billing\">
        <country>" . $request['country'] . "</country>
      </address>
      <custipaddress>" . $request['ip_address'] . "</custipaddress>
      <custnum>" . $request['customer_id'] . "</custnum>\n";
  }
  if (!empty($request['products'])) {
    $xml .= "
      <prodid>" . $request['products'] . "</prodid>";
  }
  if (isset($request['varref'])) {
    $xml .= "
      <varref>" . $request['varref'] . "</varref>";
  }
  $xml .="
    </tssinfo>
  </request>";

  return $xml;
}

/**
 * Generate a "3Dsecure verifysig" request.
 */
function _commerce_realex_remote_build_xml_request_3dsecure_verifysig($request) {
  $xml = "<request type='3ds-verifysig' timestamp='" . $request['timestamp'] . "'>
    <merchantid>" . $request['merchantid'] . "</merchantid>
    <account>" . $request['account'] . "</account>
    <orderid>" . $request['orderid'] . "</orderid>
    <amount currency='" . $request['currency'] . "'>" . $request['amount'] . "</amount>
    <card>
      <number>" . $request['ccnumber'] . "</number>
      <expdate>" . $request['expiry_date'] . "</expdate>
      <type>" . $request['cctype'] . "</type>
      <chname>" . $request['chname'] . "</chname>
    </card>
    <pares>" . $request['pares'] . "</pares>";
  if (!empty($request['comments']) && is_array($request['comments'])) {
    $xml .= "
      <comments>
        <comment id='1'>" . $request['comments'][0] . "</comment>";
    if (count($request['comments']) > 1) {
      $xml .= "<comment id='2'>" . $request['comments'][1] . "</comment>";
    }
    $xml .= "
      </comments>";
  }
  $xml .= "
    <sha1hash>" . $request['sha1hash'] . "</sha1hash>
  </request>";

  return $xml;
}

/**
 * Generate a "3Dsecure verifyenrolled" request.
 */
function _commerce_realex_remote_build_xml_request_3dsecure_verifyenrolled($request) {
  $xml = "<request type='3ds-verifyenrolled' timestamp='" . $request['timestamp'] . "'>
    <merchantid>" . $request['merchantid'] . "</merchantid>
    <account>" . $request['account'] . "</account>
    <orderid>" . $request['orderid'] . "</orderid>
    <amount currency='" . $request['currency'] . "'>" . $request['amount'] . "</amount>
    <card>
      <number>" . $request['ccnumber'] . "</number>
      <expdate>" . $request['expiry_date'] . "</expdate>
      <type>" . $request['cctype'] . "</type>
      <chname>" . $request['chname'] . "</chname>
    </card>";
  if (!empty($request['comments']) && is_array($request['comments'])) {
    $xml .= "
      <comments>
        <comment id='1'>" . $request['comments'][0] . "</comment>";
    if (count($request['comments']) > 1) {
      $xml .= "<comment id='2'>" . $request['comments'][1] . "</comment>";
    }
    $xml .= "
      </comments>";
  }
  $xml .= "
    <sha1hash>" . $request['sha1hash'] . "</sha1hash>
  </request>";

  return $xml;
}


/**
 * Generate an "dccrate" xml request.
 */
function _commerce_realex_remote_build_xml_request_dccrate($request) {
  $xml = "<request type='dccrate' timestamp='" . $request['timestamp'] . "'>
    <merchantid>" . $request['merchantid'] . "</merchantid>
    <account>" . $request['account'] . "</account>
    <orderid>" . $request['orderid'] . "</orderid>
    <amount currency='" . $request['currency'] . "'>" . $request['amount'] . "</amount>
    <card>
      <number>" . $request['ccnumber'] . "</number>
      <expdate>" . $request['expiry_date'] . "</expdate>
      <type>" . $request['cctype'] . "</type>
      <chname>" . $request['chname'] . "</chname>
      <issueno>" . $request['issueno'] . "</issueno>
    </card>
    <autosettle flag='" . $request['auto_settle'] . "'/>
    <dccinfo>
      <ccp>" . $request['dcc_provider'] . "</ccp>
      <type>1</type>
    </dccinfo>
    <sha1hash>" . $request['sha1hash'] . "</sha1hash>
  </request>";

  return $xml;
}


/**
 * Save the transaction and update the order.
 *
 * @param $payment_method
 *   Payment method array.
 * @param $order
 *   Commerce order object.
 * @param $response
 *   Response data.
 */
function commerce_realex_remote_transaction_save($payment_method, $order, $response) {
  $transaction = commerce_payment_transaction_new('commerce_realex_remote', $order->order_id);
  $transaction->amount = $order->commerce_order_total['und'][0]['amount'];
  $transaction->currency_code = $order->commerce_order_total['und'][0]['currency_code'];
  $transaction->instance_id = $payment_method['instance_id'];
  $transaction->remote_id = $response['orderid'];

  // Set the payment status.
  switch ($response['result']) {
    case '00':
      $transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
      break;
    default:
      $transaction->status = COMMERCE_PAYMENT_STATUS_FAILURE;
  }

  // Log the response variables.
  $transaction->message = 'Status @resultcode, @message. Order ID: @orderid, Auth code: @authcode, Timestamp: @timestamp, Pasref: @pasref, Address check: @address, Postcode check: @postcode, CVN result: @cvn';
  $transaction->message_variables = array(
    '@resultcode' => $response['result'],
    '@message' => $response['message'],
    '@orderid' => $response['orderid'],
    '@authcode' => $response['authcode'],
    '@timestamp' => $response['timestamp'],
    '@pasref' => $response['pasref'],
    '@address' => $response['avsaddressresponse'],
    '@postcode' => $response['avspostcoderesponse'],
    '@cvn' => $response['cvnresult'],
  );

  commerce_payment_transaction_save($transaction);

  return $transaction;
}

