<?php

namespace Drupal\commerce_realex\Controller;

use Drupal\Core\Controller\ControllerBase;
use Drupal\Core\Url;
use GlobalPayments\Api\Entities\Exceptions\ApiException;
use GlobalPayments\Api\Entities\Exceptions\BuilderException;
use GlobalPayments\Api\Entities\Exceptions\ConfigurationException;
use GlobalPayments\Api\Entities\Exceptions\GatewayException;
use GlobalPayments\Api\Entities\Exceptions\UnsupportedTransactionException;
use GlobalPayments\Api\Services\HostedService;
use GlobalPayments\Api\ServicesConfig;

// @todo ROAD-MAP use CrmPayableItemInterface formally

/**
 * Controller to handle RealexHpp Response.
 */
class RealexHppResponse extends ControllerBase {

  /**
   * The user private payment temp-store.
   *
   * @var \Drupal\user\PrivateTempStore
   */
  protected $paymentTempStore;

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
   * @return string|\Symfony\Component\HttpFoundation\RedirectResponse
   *   RedirectResponse represents an HTTP response doing a redirect.
   *
   * @throws \Drupal\Core\TempStore\TempStoreException
   */
  public function processResponse($payable_item_id) {

    try {
      $this->payableItemId = $payable_item_id;
      $this->paymentSharedTempStore = \Drupal::service('tempstore.shared')->get('commerce_realex');
      $payable = $this->paymentSharedTempStore->get($payable_item_id);
      $payable_item_class = $this->paymentSharedTempStore->get($payable_item_id)['class'];
      $this->payableItem = $payable_item_class::createFromPaymentSharedTempStore($payable_item_id);
    }
   catch (BuilderException $e) {
     // Handle builder errors.
     \Drupal::logger('commerce_realex')->error($e->getMessage());
   }
   catch (ConfigurationException $e) {
     // Handle errors related to your services configuration.
     \Drupal::logger('commerce_realex')->error($e->getMessage());
   }
   catch (GatewayException $e) {
     // Handle gateway errors/exceptions.
     \Drupal::logger('commerce_realex')->error($e->getMessage());
   }
   catch (UnsupportedTransactionException $e) {
     // Handle errors when the configured gateway doesn't support
     // desired transaction.
     \Drupal::logger('commerce_realex')->error($e->getMessage());
   }
   catch (ApiException $e) {
     // Handle all other Global Payments errors.
     \Drupal::logger('commerce_realex')->error($e->getMessage());
   }
    catch (\Exception $e) {
      // Handle all other errors.
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
      // Get type of payment method from config because of different returned data structure.
      if ($realex_config['realex_payment_method'] == 'lightbox') {
        $responseJson = $_POST['hppResponse'];
        $parsedResponse = $service->parseResponse($responseJson, true);
        $orderId = $parsedResponse->orderId; // GTI5Yxb0SumL_TkDMCAxQA
        $result = $parsedResponse->responseCode; // 00
        $globalPaymentsMessage = $parsedResponse->responseMessage; // [ test system ] Authorised
        $responseValues = $parsedResponse->responseValues; // get values accessible by key
        $pasRef = $parsedResponse->responseValues['PASREF'];
        $clean_data = stripslashes($responseValues['SUPPLEMENTARY_DATA']);
        $supplementary_data = json_decode($clean_data);
      }
      else {
        $parsedResponse = $_POST;
        $orderId = $parsedResponse['ORDER_ID'];
        $result = $parsedResponse['RESULT'];
        $globalPaymentsMessage = $parsedResponse['MESSAGE'];
        $responseValues = $parsedResponse;
        $pasRef = $parsedResponse['PASREF'];
        $clean_data = stripslashes($parsedResponse['SUPPLEMENTARY_DATA']);
        $supplementary_data = json_decode($clean_data);
        $authCode = $parsedResponse['AUTHCODE'];
      }
    }
    catch (BuilderException $e) {
      // Handle builder errors.
      \Drupal::logger('commerce_realex')->error($e->getMessage());
    }
    catch (ConfigurationException $e) {
      // Handle errors related to your services configuration.
      \Drupal::logger('commerce_realex')->error($e->getMessage());
    }
    catch (GatewayException $e) {
      // Handle gateway errors/exceptions.
      \Drupal::logger('commerce_realex')->error($e->getMessage());
    }
    catch (UnsupportedTransactionException $e) {
      // Handle errors when the configured gateway doesn't support
      // desired transaction.
      \Drupal::logger('commerce_realex')->error($e->getMessage());
    }
    catch (ApiException $e) {
      // Handle all other Global Payments errors.
      \Drupal::logger('commerce_realex')->error($e->getMessage());
    }
    catch (\Exception $e) {
      // Handle all other errors.
      \Drupal::logger('commerce_realex')->error($e->getMessage());
    }

    // Get payable Item ID out of response supplementary data.
    // We sent this to Realex in the request JSON, expect to get it back.
    $this->payableItemId = $supplementary_data->temporary_payable_item_id;

    // Retrieve object representing temporary record from paymentSharedTempStore.
    $payable_item_class = $this->paymentSharedTempStore->get($this->payableItemId)['class'];
    /* @var Drupal\commerce_realex\PayableItemInterface */
    $this->payableItem = $payable_item_class::createFromPaymentSharedTempStore($this->payableItemId);

    // Check stuff is OK.
    if ($result == '00') {
      \Drupal::logger('realex success')->error('we have a successful transaction');
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

      // Redirect the user to the "Successful Payment" callback.
      $success_callback = 'commerce_payment.checkout.return';
      return $this->redirect_back($realex_config['realex_payment_method'], TRUE);
    }

    // Otherwise something went wrong with payment.

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
    return $this->redirect_back($realex_config['realex_payment_method'], FALSE);

  }

  /**
   * Handle what happens on successful transaction.
   *
   * Lightbox needs a Redirect response.
   * Redirect needs HTML response.
   *
   * Parameter: $method string
   */
  public function redirect_back($method, $success) {
    $success_callback = 'commerce_payment.checkout.return';
    $failure_callback = 'commerce_realex.payment_failure';
    if ($success) {
      $callback = $success_callback;
    }
    else {
      $callback = $failure_callback;
    }
    // Redirect requires HTML to be returned. Lightbox can, ironically, handle a redirect.
    if ($method == 'redirect') {
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
      print $html;
      return '';
    }
    else {
      \Drupal::logger('realex redirect')
      ->notice('doing lightbox redirect');
      // We'll make Lightbox the default for now.
      return $this->redirect($callback,
       [
          'commerce_order' => $this->payableItem->getValue('commerce_order_id'),
          'step' => 'payment',
        ],
        ['query' => ['payable_item_id' => $this->payableItemId],]
      );
    }
  }
}
