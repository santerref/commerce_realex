<?php

namespace Drupal\commerce_realex\Controller;

use com\realexpayments\hpp\sdk\domain\HppRequest;
use com\realexpayments\hpp\sdk\RealexHpp;
use com\realexpayments\hpp\sdk\RealexValidationException;
use com\realexpayments\hpp\sdk\RealexException;
use Drupal\Core\Controller\ControllerBase;
use Symfony\Component\HttpFoundation\Response;

class RealexHppRequest extends ControllerBase {

  /**
   * @var \Drupal\user\PrivateTempStore
   */
  protected $paymentTempStore;

  /**
   * @var Drupal\commerce_realex\PayableItemInterface
   */
  protected $payableItem;

  /**
   * @var string
   */
  protected $payableItemId;

  /**
   * Build a RealEx HPP request JSON object.
   */
  public function buildJson($payable_item_id) {

    try {
      $this->payableItemId = $payable_item_id;
      $this->paymentTempStore = \Drupal::service('user.private_tempstore')->get('commerce_realex');
      $payable_item_class = $this->paymentTempStore->get($payable_item_id)['class'];
      $this->payableItem = $payable_item_class::createFromPaymentTempStore($payable_item_id);
    }
    catch (\Exception $e) {
      \Drupal::logger('commerce_realex')->error($e->getMessage());
    }

    $realex_config = $this->payableItem->getValue('realex_config');
    $supplementary_data = [
      'temporary_payable_item_id' => $this->payableItemId,
			'HPP_CUSTOMER_FIRSTNAME' => $this->payableItem->getValue('given_name'),
			'HPP_CUSTOMER_LASTNAME' => $this->payableItem->getValue('family_name')
    ];
    $hppRequest = (new HppRequest())
      ->addMerchantId($realex_config['realex_merchant_id'])
      ->addAccount($realex_config['realex_account'])
      ->addAmount($this->payableItem->getPayableAmount())  // This is cents not Euros.
      ->addSupplementaryData($supplementary_data)
      ->addCurrency($this->payableItem->getValue('payable_currency'))
      ->addOrderId($this->payableItemId) // This is the Temp Payable ID.
      ->addCommentOne($this->payableItem->getValue('commerce_order_id'))
      ->addCustomerNumber($this->payableItem->getValue('payable_uid')) // User's ID
      ->addAutoSettleFlag(TRUE);

    $realexHpp = new RealexHpp($realex_config['realex_shared_secret']);

    try {
      $requestJson = $realexHpp->requestToJson($hppRequest);
      $response = new Response();
      $response->setContent($requestJson);
      $response->headers->set('Content-Type', 'application/json');
      return $response;
    }
    catch (RealexValidationException $e) {
      \Drupal::logger('commerce_realex')->error($e->getMessage());
    }
    catch (RealexException $e) {
      \Drupal::logger('commerce_realex')->error($e->getMessage());
    }
    catch (\Exception $e) {
      \Drupal::logger('commerce_realex')->error($e->getMessage());
    }

  }
}
