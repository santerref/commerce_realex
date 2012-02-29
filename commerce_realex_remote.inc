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

  $form['auto_settle'] = array(
    '#type' => 'select',
    '#title' => t('Auto settle flag'),
    '#options' => array(1 => t('Enabled'), 0 => t('Disabled')),
    '#default_value' => isset($settings['auto_settle']) ?  $settings['auto_settle'] : 1,
    '#required' => TRUE,
  );

  return $form;
}

/**
 * Implements hook_commerce_payment_method_submit_form().
 *
 * Set allowed credit card types.
 */
function commerce_realex_remote_commerce_payment_method_submit_form($payment_method, $pane_values, $checkout_pane, $order) {
  module_load_include('inc', 'commerce_payment', 'includes/commerce_payment.credit_card');

  // Set which cards are available to the user.
  $credit_card_settings = array(
    'owner' => '',
    'type' => array(
      'visa',
      'mastercard',
      'amex',
      'switch',
      'solo',
      'delta',
      'dc',
      'laser',
    ),
    'number' => '',
    'start_date' => '',
    'issue' => '',
    'code' => '',
  );

  return commerce_payment_credit_card_form($credit_card_settings);
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

  if (!commerce_payment_credit_card_validate($pane_values['credit_card'], $settings)) {
    return FALSE;
  }
}

/**
 * Implements hook_commerce_payment_method_submit_form_submit().
 */
function commerce_realex_remote_commerce_payment_method_submit_form_submit($payment_method, $pane_form, $pane_values, &$order, $charge) {
  $start_date = sprintf("%02d%02d", $pane_values['credit_card']['start_month'], substr($pane_values['credit_card']['start_year'], -2));
  $expiry_date = sprintf("%02d%02d", $pane_values['credit_card']['exp_month'], substr($pane_values['credit_card']['exp_year'], -2));

  $request = array();
  $request['timestamp'] = strftime('%Y%m%d%H%M%S');
  mt_srand((double) microtime() * 1000000);

  $request['order_number'] = $order->order_number;
  $request['orderid'] = $request['order_number'] . $request['timestamp'];
  $request['merchantid'] = $payment_method['settings']['merchant_id'];
  $request['account'] = $payment_method['settings']['account_id'];
  $request['secret'] = $payment_method['settings']['shared_secret'];
  $request['auto_settle'] = $payment_method['settings']['auto_settle'];
  $request['chname'] = $pane_values['credit_card']['owner'];
  $request['cctype'] = _commerce_realex_get_card_type($pane_values['credit_card']['type']);
  $request['ccnumber'] = $pane_values['credit_card']['number'];
  $request['cvn'] = $pane_values['credit_card']['code'];
  $request['issueno'] = isset($pane_values['credit_card']['issue']) ? $pane_values['credit_card']['issue'] : '';
  $request['start_date'] = $start_date;
  $request['expiry_date'] = $expiry_date;

  // Load the customer profile for the order.
  $profile = commerce_customer_profile_load($order->commerce_customer_billing['und'][0]['profile_id']);
  $address = $profile->commerce_customer_address['und'][0];
  $request['customer_id'] = $order->commerce_customer_billing['und'][0]['profile_id'];
  $request['customer_name'] = $address['name_line'];
  $request['street_one'] = $address['thoroughfare'];
  $request['postcode'] = $address['postal_code'];
  $request['city'] = $address['locality'];
  $request['region'] = $address['administrative_area'];
  $request['country'] = $address['country'];
  $request['amount'] = $charge['amount'];
  $request['currency'] = $charge['currency_code'];

  // Get products purchased.
  $products = array();
  foreach ($order->commerce_line_items as $delta => $line_items) {
    foreach ($line_items as $id => $line_item) {
      $products[] = $line_item['line_item_id'];
    }
  }
  $request['products'] = implode(',', $products);

  // Build sha1 hash.
  $hash = $request['timestamp'] . '.' . $request['merchantid'] . '.' . $request['orderid'] . '.' . $request['amount'] . '.' . $request['currency'] . '.' . $request['ccnumber'];
  $sha1hash = sha1($hash);
  $hash = $sha1hash . '.' . $request['secret'];
  $request['sha1hash'] = sha1($hash);

  // Send the request.
  $url = 'https://epage.payandshop.com/epage-remote.cgi';
  $xml = _commerce_realex_build_xml_request($request);
  $response = _commerce_realex_send_request($url, $xml);

  // Process response.
  _commerce_realex_process_response($request, $response, $order, $payment_method);
}

