<?php

namespace Drupal\commerce_realex\Controller;

use Drupal\Core\Controller\ControllerBase;
use Drupal\Core\Url;
use GlobalPayments\Api\Services\HostedService;
use GlobalPayments\Api\ServicesConfig;
use Symfony\Component\HttpFoundation\Response;

// @todo ROAD-MAP use CrmPayableItemInterface formally

/**
 * Controller to handle RealexHpp Response.
 */
class RealexHppResponse extends ControllerBase {

  /**
   * The shared payment temp-store.
   *
   * @var \Drupal\Core\TempStore\SharedTempStore
   */
  protected $paymentSharedTempStore;

  /**
   * The shared failed payment temp-store.
   *
   * @var \Drupal\Core\TempStore\SharedTempStore
   */
  protected $paymentFailureTempStore;

  /**
   * The payable item.
   *
   * @var \Drupal\commerce_realex\PayableItemInterface
   */
  protected $payableItem;

  /**
   * The payable item UUID.
   *
   * @var string
   */
  protected $payableItemId;

  /**
   * The parsed response from Global Payments.
   *
   * @var \GlobalPayments\Api\Entities\Transaction
   */
  protected $hppResponse;

  /**
   * Process a HPP payment response from Global Payments.
   *
   * @param string $payable_item_id
   *   The payable item UUID.
   *
   * @return \Symfony\Component\HttpFoundation\Response|\Symfony\Component\HttpFoundation\RedirectResponse
   *   Either a Response containing HTML or a RedirectResponse representing an
   *   HTTP response doing a redirect.
   *
   * @throws \Drupal\Core\TempStore\TempStoreException
   */
  public function processResponse($payable_item_id) {
    try {
      $this->payableItemId = $payable_item_id;
      $this->paymentSharedTempStore = \Drupal::service('tempstore.shared')
        ->get('commerce_realex');
      $payable_item_class = $this->paymentSharedTempStore->get($payable_item_id)['class'];
      $this->payableItem = $payable_item_class::createFromPaymentSharedTempStore($payable_item_id);
    }
    catch (\Exception $e) {
      \Drupal::logger('commerce_realex')->error($e->getMessage());
    }

    $realex_config = $this->payableItem->getValue('realex_config');

    // Configure client settings.
    $config = new ServicesConfig();
    $config->merchantId = $realex_config['realex_merchant_id'];
    $config->accountId = $realex_config['realex_account'];
    $config->sharedSecret = $realex_config['realex_shared_secret'];
    $config->serviceUrl = $realex_config['realex_server_url'];

    $service = new HostedService($config);

    try {
      // Create the response object from the response JSON.
      // Get type of payment method from config because of different returned
      // data structure.
      if ($realex_config['realex_payment_method'] === 'lightbox') {
        $responseJson = $_POST['hppResponse'];
        $parsedResponse = $service->parseResponse($responseJson, TRUE);
        $result = $parsedResponse->responseCode;
        $globalPaymentsMessage = $parsedResponse->responseMessage;
        $responseValues = $parsedResponse->responseValues;
        $clean_data = stripslashes($responseValues['SUPPLEMENTARY_DATA']);
        $supplementary_data = json_decode($clean_data, FALSE);
      }
      else {
        $parsedResponse = $_POST;
        $result = $parsedResponse['RESULT'];
        $globalPaymentsMessage = $parsedResponse['MESSAGE'];
        $clean_data = stripslashes($parsedResponse['SUPPLEMENTARY_DATA']);
        $supplementary_data = json_decode($clean_data, FALSE);
        $authCode = $parsedResponse['AUTHCODE'];
      }
    }
    catch (\Exception $e) {
      \Drupal::logger('commerce_realex')->error($e->getMessage());
    }

    // Get payable Item ID out of response supplementary data.
    // We sent this to Realex in the request JSON, expect to get it back.
    $this->payableItemId = $supplementary_data->temporary_payable_item_id;

    // Retrieve object representing temporary record from
    // paymentSharedTempStore.
    $payable_item_class = $this->paymentSharedTempStore->get($this->payableItemId)['class'];
    /* @var \Drupal\commerce_realex\PayableItemInterface */
    $this->payableItem = $payable_item_class::createFromPaymentSharedTempStore($this->payableItemId);

    // Check stuff is OK.
    if ($result == '00') {
      \Drupal::logger('realex success')
        ->error('we have a successful transaction');
      // Display a message.
      // @todo - Allow user to override this.
      $currency_formatter = \Drupal::service('commerce_price.currency_formatter');
      $payment_amount_formatted = $currency_formatter->format($this->payableItem->getValue('payable_amount'), $this->payableItem->getValue('payable_currency'));
      $display_message = $this->t('Thank you for your payment of @payment_amount.', ['@payment_amount' => $payment_amount_formatted]);
      \Drupal::messenger()->addStatus($display_message);

      // Update PayableItem in temp store.
      $this->payableItem->setValue('payment_complete', TRUE);
      $this->payableItem->setValue('authCode', $authCode);
      $this->payableItem->setValue('message', $globalPaymentsMessage);
      $this->payableItem->saveSharedTempStore($this->payableItemId);

      return $this->redirectBack($realex_config['realex_payment_method'], TRUE);
    }

    // Otherwise something went wrong with payment.
    //
    // @todo tell the user the outcome!
    // @todo get human-readable message from realex response
    // @todo take action based on which realex response we get
    // @todo - invalid card details
    // @todo - set message, redirect to payment form to attempt again?
    // @todo - handle other types of failure, more specific?
    // Fallback message if no other remedial action has already been taken.
    // Store the response and redirect to callback.
    $this->paymentFailureTempStore = \Drupal::service('tempstore.shared')
      ->get('commerce_realex_failure');
    $this->paymentFailureTempStore->set('payment', $parsedResponse);

    return $this->redirectBack($realex_config['realex_payment_method'], FALSE);
  }

  /**
   * Handle what happens on transaction.
   *
   * Lightbox needs a Redirect response.
   * Redirect needs HTML response.
   *
   * @param string $method
   *   The method which was used for the payment.
   * @param bool $success
   *   TRUE if the transaction was successful, FALSE otherwise.
   *
   * @return \Symfony\Component\HttpFoundation\Response|\Symfony\Component\HttpFoundation\RedirectResponse
   *   Either a Response containing HTML for method 'redirect' or a
   *   RedirectResponse representing an HTTP response doing a redirect
   *   otherwise.
   */
  public function redirectBack($method, $success) {
    $success_callback = 'commerce_payment.checkout.return';
    $failure_callback = 'commerce_realex.payment_failure';
    if ($success) {
      $callback = $success_callback;
    }
    else {
      $callback = $failure_callback;
    }
    // Redirect requires HTML to be returned. Lightbox can, ironically, handle
    // a redirect.
    if ($method === 'redirect') {
      if ($success) {
        $url = Url::fromRoute($callback,
          [
            'commerce_order' => $this->payableItem->getValue('commerce_order_id'),
            'step' => 'payment',
          ],
          [
            'query' => ['payable_item_id' => $this->payableItemId],
            'absolute' => TRUE,
          ]
        );
      }
      else {
        $url = Url::fromRoute($callback,
          [
            'payable_item_id' => $this->payableItemId,
          ],
          [
            'absolute' => TRUE,
          ]
        );
      }
      $html = '<style type="text/css">body {display: none;}</style>';
      $html .= '<script type="text/javascript"> window.location = "' . $url->toString() . '";</script>';

      $response = new Response(
        $html,
        Response::HTTP_OK,
        ['content-type' => 'text/html']
      );

      return $response;
    }

    \Drupal::logger('realex redirect')->notice('doing lightbox redirect');

    // We'll make Lightbox the default for now.
    return $this->redirect($callback,
      [
        'commerce_order' => $this->payableItem->getValue('commerce_order_id'),
        'step' => 'payment',
      ],
      ['query' => ['payable_item_id' => $this->payableItemId]]
    );
  }

}
