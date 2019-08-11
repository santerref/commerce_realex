<?php

namespace Drupal\commerce_realex;

// @todo ROAD-MAP use PayableItemInterface formally.
// use Drupal\commerce_realex\PayableItemInterface;

/**
 * Represents a Payable Item.
 */
class PayableItem {

  /**
   * @var \Drupal\user\PrivateTempStore
   */
  protected $paymentTempStore;

  /**
   * Array of key:value.
   *
   * @var array
   *
   * @todo Use separate class members instead of an array?
   *
   * @todo ROAD MAP
   *  - abstract this to a PayableItemInterface?
   */
  protected $values;

  /**
   * Constructor.
   */
  public function __construct() {
    // @todo Use dependency injection to obtain these services.
    $this->paymentTempStore = \Drupal::service('user.private_tempstore')->get('commerce_realex');
  }

  /**
   * Save a PayableItem in PrivateUsertempStore.
   *
   * We don't need to save anything about the Payment itself permanently.
   * Items using this class can use the Global Payments Order ID as payment ID.
   *
   * @return string
   *   The key the object was stored under in the Private User temp Store;
   *
   * @todo corresponding method to create from temp store, by UUID.
   *
   * @todo ROAD MAP
   *  - abstract some of this to a PermitInterface?
   */
  public function saveTempStore($uuid = NULL) {
    $uuid_service = \Drupal::service('uuid');
    if (!$uuid) {
      $uuid = $uuid_service->generate();
    }

    // Build temporary storage object from essential data.
    $storage_data = [
      'class' => __CLASS__,
      'values' => $this->values,
    ];

    // Save it to private temp store under the UUID "payment object" key.
    $this->paymentTempStore->set($uuid, $storage_data);

    return $uuid;
  }

  /**
   * Retrieve a payable from Private User Temp Store.
   *
   * @param string $uuid
   *   A UUID previously used to store data in the $paymentTempStore.
   *
   * @return Drupal\commerce_realex\PayableItem
   *
   * @todo ROAD-MAP
   *   - formalize this in some interface, e.g. PayableItem
   */
  public static function createFromPaymentTempStore(string $uuid) {
    $payable = new static();

    $temp_item = $payable->paymentTempStore->get($uuid);

    // @todo validate that the $temp_item['class'] matches __CLASS__
    foreach ($temp_item['values'] as $key => $value) {
      $payable->setValue($key, $value);
    }

    return $payable;
  }

  /**
   * Set a value.
   *
   * @param string $key
   *   A field key.
   * @param mixed $value
   *   A value to set.
   */
  public function setValue($key, $value) {
    // @todo validation?
    $this->values[$key] = $value;
  }

  /**
   * General purpose getter.
   *
   * @param string $key
   *   A field key.
   *
   * @return mixed
   *   A value to set.
   */
  public function getValue($key) {
    return $this->values[$key];
  }

  /**
   * {@inheritdoc}
   */
  public function getPayableAmount() {
    // @todo Could this be more robust by looking up the permit type (GUID)?
    if (isset($this->values['payable_amount'])) {
      // Global Payments payments wants cents not euros.
      $cents = $this->values['payable_amount'] * 100;
      return (int) $cents;
    }
    else {
      throw new \Exception('Amount not set.');
    }
  }
}
