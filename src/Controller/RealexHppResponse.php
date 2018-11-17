<?php

namespace Drupal\commerce_realex\Controller;

use com\realexpayments\hpp\sdk\domain\HppRequest;
use com\realexpayments\hpp\sdk\RealexHpp;
use com\realexpayments\hpp\sdk\RealexValidationException;
use com\realexpayments\hpp\sdk\RealexException;
use Drupal\Core\Controller\ControllerBase;
use Drupal\Core\Routing\TrustedRedirectResponse;
// @todo ROAD-MAP use CrmPayableItemInterface formally
// use Drupal\commerce_realex\CrmPayableItemInterface;
use Drupal\commerce_realex\CrmPaymentRecord;

class RealexHppResponse extends ControllerBase {

  /**
   * @var \Drupal\user\PrivateTempStore
   */
  protected $paymentTempStore;

  /**
   * @var CrmPayableItemInterface
   */
  protected $payableItem;

  /**
   * @var string
   */
  protected $payableItemId;

  /**
   * The parsed response from Realex.
   *
   * @var com\realexpayments\hpp\sdk\domain\HppResponse
   */
  protected $hppResponse;

  /**
   * Process a HPP payment respnse from RealEx.
   */
  public function processResponse($payable_item_id) {


    try {
      $this->payableItemId =  $payable_item_id;
      $this->paymentTempStore = \Drupal::service('user.private_tempstore')->get('commerce_realex');
      $payable_item_class = $this->paymentTempStore->get($payable_item_id)['class'];
      $this->payableItem = $payable_item_class::createFromPaymentTempStore($payable_item_id);
    }
    catch (\Exception $e) {
      \Drupal::logger('commerce_realex')->error($e->getMessage());
    }
    $realex_config = $this->payableItem->getValue('realex_config');

    // Parse the Realex response sent by the client-side library
    $realexHpp = new RealexHpp($realex_config['realex_shared_secret']);
    $responseJson = $_POST['hppResponse'];
    try {
      $this->hppResponse = $realexHpp->responseFromJson($responseJson);
      $result = $this->hppResponse->getResult(); // 00
      $message = $this->hppResponse->getMessage(); // [ test system ] Authorised
      $authCode = $this->hppResponse->getAuthCode(); // 12345
      $pasRef  = $this->hppResponse->getPasRef(); // 12345
      $supplementary_data = $this->hppResponse->getSupplementaryData();
    }
    catch (RealexValidationException $e) {
      return $e->getMessage();
    }
    catch (RealexException $e) {
      return $e->getMessage();
    }

    // Get payable Item ID out of response supplementary data.
    // We sent this to Realex in the request JSON, expect to get it back.
    $this->payableItemId = $supplementary_data['temporary_payable_item_id'];

    // Retrieve object representing temporary record from paymentTempStore.
    $payable_item_class = $this->paymentTempStore->get($this->payableItemId)['class'];
    /* @var Drupal\commerce_realex\CrmPayableItemInterface */
    $this->payableItem = $payable_item_class::createFromPaymentTempStore($this->payableItemId);


    // Check stuff is OK
    if ($result == '00') {

      // Display a message.
      // @todo - Allow user to override this.
      $payment_amount_formatted = sprintf('%0.2f', $this->payableItem->getValue('payable_amount'));
      $message = $this->t('Thank you for your payment of €@payment_amount.',
                          ['@payment_amount' => $payment_amount_formatted]);
      drupal_set_message($message);

      // Update PayableItem in tempstore.
      $this->payableItem->setValue('payment_complete', TRUE);
      $this->payableItem->saveTempStore($this->payableItemId);

      // Redirect the user to the "Successfull Payment" callback.
			$success_callback = 'commerce_payment.checkout.return';
			return $this->redirect($success_callback,
				[
					'commerce_order' => $this->payableItem->getValue('commerce_order_id'),
					'step' => 'payment',
				],
				['query' => ['payable_item_id' => $this->payableItemId],]
			);

    }
    else {
    // otherwise something went wrong with payment

    // @todo tell the user the outcome!
    // get human-readable message from realex response
    // take action based on which realex response we get
    //  - invalid card details
    //    - set message, redirect to payment form to attempt again?
    //  - handle other types of failure, more specific?

    // fallback message if no other remedial action has already been taken.
      // Store the response and redirect to callback.
      $this->paymentFailureTempStore = \Drupal::service('user.private_tempstore')->get('commerce_realex_failure');
      $this->paymentFailureTempStore->set('payment', $this->hppResponse);
      return $this->redirect('commerce_realex.payment_failure',
        ['payable_item_id' => $this->payableItemId]
      );
    }

  }

}
