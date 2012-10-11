<?php

/**
 * Implements hook_checkout_form().
 */
function commerce_realex_remote_dcc_pane_checkout_form($form, &$form_state, $checkout_pane, $order) {
  $pane_form = array();

  // Don't display this checkout pane if the payment method isn't Realex Remote.
  if ($order->data['payment_method'] != 'commerce_realex_remote|commerce_payment_commerce_realex_remote') {
    return $pane_form;
  }

  // Only display DCC offer if (a) DCC is enabled and (b) we have a valid offer.
  $payment_method = commerce_payment_method_instance_load($order->data['payment_method']);
  if ($payment_method['settings']['dcc_provider'] != 'none' && !empty($order->data['commerce_realex_dcc_message'])) {
    $dcc_message = $order->data['commerce_realex_dcc_message'];

    $form['dcc'] = array(
      '#type' => 'fieldset',
      '#title' => t('Direct Currency Conversion'),
      '#collapsible' => FALSE,
      '#weight' => 10,
    );

    $pane_form['dcc_message'] = array(
      '#type' => 'markup',
      '#markup' => '<p>' . $dcc_message . '</p>',
      '#prefix' => '<div id="dcc-info">',
      '#suffix' => '</div>',
    );

    $pane_form['dcc_accept'] = array(
      '#type' => 'checkbox',
      '#title' => t('Yes - charge me in my own currency'),
      '#required' => FALSE,
    );
  }

  return $pane_form;
}

/**
 * Implements hook_checkout_form_submit().
 */
function commerce_realex_remote_dcc_pane_checkout_form_submit($form, &$form_state, $checkout_pane, $order) {
  if ($order->data['payment_method'] != 'commerce_realex_remote|commerce_payment_commerce_realex_remote') {
    return;
  }

return;
  // TODO: get $pane_values['credit_card']
  $payment_method = commerce_payment_method_instance_load($order->data['payment_method']);

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
  $request['merchantid'] = $payment_method['settings']['merchant_id'];
  $request['account'] = $payment_method['settings']['account_id'];
  $request['secret'] = $payment_method['settings']['shared_secret'];
  $request['auto_settle'] = $payment_method['settings']['auto_settle'];
  $request['dcc_provider'] = $payment_method['settings']['dcc_provider'];

  $request['chname'] = $pane_values['credit_card']['owner'];
  $request['cctype'] = _commerce_realex_get_card_type($pane_values['credit_card']['type']);
  $request['ccnumber'] = $pane_values['credit_card']['number'];
  $request['cvn'] = $pane_values['credit_card']['code'];
  $request['issueno'] = isset($pane_values['credit_card']['issue']) ? $pane_values['credit_card']['issue'] : '';
  $request['start_date'] = $start_date;
  $request['expiry_date'] = $expiry_date;

  // Load the customer profile for the order.
  $profile = commerce_customer_profile_load($order->commerce_customer_billing['und'][0]['profile_id']);
  $address = $profile->commerce_customer_address['und'][0];  $request['customer_id'] = $order->commerce_customer_billing['und'][0]['profile_id'];
  $address = $profile->commerce_customer_address['und'][0];  $request['customer_id'] = $order->commerce_customer_billing['und'][0]['profile_id'];
  $request['customer_name'] = $address['name_line'];
  $request['street_one'] = $address['thoroughfare'];
  $request['postcode'] = $address['postal_code'];
  $request['city'] = $address['locality'];
  $request['region'] = $address['administrative_area'];
  $request['country'] = $address['country'];
  $request['amount'] = $charge['amount'];
  $request['currency'] = $charge['currency_code'];
  $request['ip_address'] = ip_address();

  // TODO - fix, not line item ids, get product ids!!
  // Get products purchased.
  $products = array();
  foreach ($order->commerce_line_items as $delta => $line_items) {
    foreach ($line_items as $id => $line_item) {
      $products[] = $line_item['line_item_id'];
    }
  }
  $request['products'] = implode(',', $products);

  $pane_id = $checkout_pane['commerce_realex_remote_dcc_pane'];
  // Pane wasn't displayed as DCC offer unsuccessful.
  if (!isset($form_state['values'][$pane_id]['dcc_accept'])) {
    $request['dcc'] = FALSE;
  }
  // Customer does not accept the rate request.
  elseif (!$form_state['values'][$pane_id]['dcc_accept']) {
    $request['dcc'] = TRUE;
    $request['cardholdercurrency'] = $request['currency'];
    $request['cardholderamount'] = $request['amount'];
    $request['cardholderrate'] = 1;
  }
  // Customer accepts the rate request.
  else {
    $request['dcc'] = TRUE;
    $request['cardholdercurrency'] = $_SESSION['cardholdercurrency'];
    $request['cardholderamount'] = $_SESSION['cardholderamount'];
    $request['cardholderrate'] = $_SESSION['cardholderrate'];
  }

  // Build sha1 hash.
  $request['sha1hash'] = _commerce_realex_build_request_sha1hash($request);

  module_load_include('inc', 'commerce_realex', 'commerce_realex_remote');
  commerce_realex_remote_do_auth_request($payment_method, $request, $order);
}

