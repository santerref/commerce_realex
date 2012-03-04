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

  $form['auto_settle'] = array(
    '#type' => 'select',
    '#title' => t('Auto settle flag'),
    '#options' => array(1 => t('Enabled'), 0 => t('Disabled')),
    '#default_value' => isset($settings['auto_settle']) ?  $settings['auto_settle'] : 1,
    '#required' => TRUE,
  );

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

  // Get credit card fields.
  $form = commerce_payment_credit_card_form($credit_card_settings);

  // Add DCC checkbox.
  if ($payment_method['settings']['dcc_provider'] != 'none') {
    $form['credit_card']['dcc'] = array(
      '#type' => 'fieldset',
      '#title' => t('Direct Currency Conversion'),
      '#collapsible' => FALSE,
      '#weight' => 10,
    );

    // Get order info.
    $wrapper = entity_metadata_wrapper('commerce_order', $order);
    $charge['amount'] = $wrapper->commerce_order_total->amount->value();
    $charge['currency_code'] = $wrapper->commerce_order_total->currency_code->value();

    // TODO
    // $pane_values is empty. Don't have credit card details yet as the user
    // hasn't completed the form yet. Need another step? Move payment widget to
    // checkout page?
    $dcc_message = commerce_realex_remote_get_dcc_offer($payment_method, $pane_values, $order, $charge);
    $form['credit_card']['dcc']['dcc_message'] = array(
      '#type' => 'markup',
      '#markup' => '<p>' . $dcc_message . '</p>',
    );

    $form['credit_card']['dcc']['dcc_accept'] = array(
      '#type' => 'checkbox',
      '#title' => t('Yes - charge me in my own currency'),
      '#required' => FALSE,
    );
  }

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
  $request['dcc_provider'] = $payment_method['settings']['dcc_provider'];

  $request['dcc_accept'] = $pane_values['credit_card']['dcc_accept'];
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

  // Build sha1 hash.
  $hash = $request['timestamp'] . '.' . $request['merchantid'] . '.' . $request['orderid'] . '.' . $request['amount'] . '.' . $request['currency'] . '.' . $request['ccnumber'];
  $sha1hash = sha1($hash);
  $hash = $sha1hash . '.' . $request['secret'];
  $request['sha1hash'] = sha1($hash);

  // DCC Info.
  if ($request['dcc_provider'] != 'none') {
    // Customer does not accept the rate request.
    if (!$request['dcc_accept']) {
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
  }

  // Send the request.
  $url = $payment_method['settings']['remote_url'];
  $type = 'auth';
  $xml = _commerce_realex_remote_build_xml_request($request, $type);
  $response = _commerce_realex_send_request($url, $xml);

  // Verify response.
  $success = _commerce_realex_remote_verify_response($request, $response, $order, $payment_method);

  // Successful payment.
  if ($success) {
    // Save the transaction.
    $transaction = commerce_realex_remote_transaction_save($payment_method, $order, $response);
    $order->data['commerce_realex_remote'] = $pane_values;
    $order->data['commerce_realex_remote']['transaction'] = $transaction;
  }

  // Payment failure.
  else {
    drupal_set_message(t('We were unable to process your credit card payment; reason provided by bank: %message. If the problem persists, contact us to complete your order.', array('%message' => $message)), 'warning');
    watchdog('commerce_realex', 'Payment failed: %message', array('%message' => $message), WATCHDOG_WARNING);
    $_SESSION['realex_failures']++;
    drupal_goto('checkout');
  }
}


/**
 * Contact Realex to get a DCC offer.
 */
//function commerce_realex_remote_get_dcc_offer(&$order) {
function commerce_realex_remote_get_dcc_offer($payment_method, $pane_values, $order, $charge) {
  // TODO - start
  /*
  // Need to decrypt data to get card number.
  $key = uc_credit_encryption_key();
  $crypt = new uc_encryption_class;

  // Save the unencrypted CC details for the duration of this request.
  // In recent versions, we base64_encode() the payment details before
  // encrypting. We can detect encoded data by the lack of colons,
  // due to base64's limited character set.
  $data = $crypt->decrypt($key, $_SESSION['sescrd']);
  if (strpos($data, ':') === FALSE) {
    $data = base64_decode($data);
  }
  $payment_details = unserialize($data);
  */
  // TODO - end

  $start_date = sprintf("%02d%02d", $pane_values['credit_card']['start_month'], substr($pane_values['credit_card']['start_year'], -2));
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
  $request['amount'] = $charge['amount'];
  $request['currency'] = $charge['currency_code'];

  // Build sha1 hash.
  $hash = $request['timestamp'] . '.' . $request['merchantid'] . '.' . $request['orderid'] . '.' . $request['amount'] . '.' . $request['currency'] . '.' . $request['ccnumber'];
  $sha1hash = sha1($hash);
  $hash = $sha1hash . '.' . $request['secret'];
  $request['sha1hash'] = sha1($hash);

  // Send the request.
  $url = $payment_method['settings']['remote_url'];
  $type = 'dccrate';
  $xml = _commerce_realex_remote_build_xml_request($request, $type);
  $response = _commerce_realex_send_request($url, $xml);

  // Verify response.
  $success = _commerce_realex_remote_verify_response($request, $response, $order, $payment_method);

  // Failed to get DCC offer, return.
  if (!$success) {
    return '';
  }

  // Received DCC offer, proceed.
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

  return $message;
}

/**
 * Process the Realex response.
 *
 * @param $response
 *   Parsed response as an array.
 * @param $request
 *   Transaction details array.
 * @param $order
 *   Commerce order object.
 * @param $payment_method
 *   Payment method array.
 */
function _commerce_realex_remote_verify_response($request, $response, $order, $payment_method) {
  if (!isset($response['result'])) {
    watchdog('commerce_realex', 'No results code, something has gone wrong with the transaction.', array(), WATCHDOG_ERROR);
    drupal_goto('checkout/' . $order->order_number . '/payment');
  }

  $merchantid = $request['merchantid'];
  $secret = $request['secret'];
  $orderid = $request['orderid'];
  $timestamp = $response['timestamp'];
  $resultcode = $response['result'];
  $message = $response['message'];
  $pasref = $response['pasref'];
  $authcode = $response['authcode'];

  // This digital signature should correspond to the one Realex Payments returns
  // and can therefore be used to verify the message Realex sends back.
  $tmp = "$timestamp.$merchantid.$orderid.$resultcode.$message.$pasref.$authcode";
  $sha1hash = sha1($tmp);
  $tmp = "$sha1hash.$secret";
  $sha1hash = sha1($tmp);

  $success = FALSE;
  $transaction = NULL;
  switch ($response['result']) {
    case '00':
      // Check to see if hashes match or not
      if ($sha1hash != $response['sha1hash']) {
        watchdog('commerce_realex', 'Hash match failed for order @order_id.', array('@order_id' => $response['orderid']));
        drupal_set_message(t("Hashes don't match - response not authenticated!"), 'error');
        $message = 'Bank connection error, please try again later';
        $success = FALSE;
      }
      else {
        $success = TRUE;
      }
      break;

    default:
      $success = FALSE;
      break;
  }

  // Unlikely, but just in case they can ever decrypt it to get card number.
  unset($result['sha1hash']);

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
      <chname>" . $request['chname'] . "</chname>
      <issueno>" . $request['issueno'] . "</issueno>
    </card>";
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
  $xml .= "
    <autosettle flag='" . $request['auto_settle'] . "'/>
    <sha1hash>" . $request['sha1hash'] . "</sha1hash>
    <tssinfo>
      <address type=\"billing\">
        <country>" . $request['country'] . "</country>
      </address>
      <custipaddress>" . $request['ip_address'] . "</custipaddress>
      <custnum>" . $request['customer_id'] . "</custnum>
      <prodid>" . $request['products'] . "</prodid>
    </tssinfo>
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

