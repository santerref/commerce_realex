<?php

namespace Drupal\commerce_realex;

/**
 * Represents a thing which can be paid for, in the context of the Website.
 *
 * Example, a Dog Permit is something that can be paid for, so it would
 * implement this inteface.
 *
 * @todo ROAD-MAP
 *   This is a sketch of the methods that will be useful for making a generic
 *   interface for payable items in the Website, not limited to licences & permits.
 *   In particular, classes implementing PayableItemInterface may have
 *   distinct machine-names for CRM relationship fields (permit-to-customer,
 *   payment-to-permit, etc.)
 *
 * @todo ROAD-MAP
 *   Start using this formally.  So far, nothing officially implements it,
 *   though many of these methods are already in use.
 */
interface PayableItemInterface {

  /**
   * A set of methods to prepare suitable human-readable strings for display on
   * payment forms, Realex fields, CRM record titles & descriptions, etc.
   *
   * @return string
   */
  // public function getPayableItemTitle();
  // public function getPaymentTypeName();
  // public getPayableItemDescription();

  /**
   * Prepare a summery of the payable item properties for display, in payment
   * checkout steps, order success reciepts, listings, etc.
   *
   * maybe numerous similar methods, e.g long summary, one-line summary,
   * table row for listing, etc.
   *
   * @return render array
   */
  // public getRenderSummary();

  /**
   * Get the name of a Realex Payment Account for this payable item type.
   *
   * @return string
   *   Name of Realex Payment Account.
   */
  // public function getPaymentAccount();

  /**
   * Get the price of the payableItem.
   *
   * @return integer
   *   Price in cents.
   *
   * @todo equivalent for decimal amount in Euros
   */
  // public function getFee();

  /**
   * Retrieve a PayableItem from temp store.
   *
   * @param $payable_item_id
   *   UUID of payable_item_id in temp store.
   */
  // public function getPayableItemFromTempStore($payable_item_id);

  /**
   * Save a CrmPayableItem in temp store.
   *
   * @return string
   *   UUID of payable_item_id in temp store.
   */
  // public function savePayableItemInTempStore();

  /**
   * Get a summary of the payment item.
   *
   * Intended for a payment form to be able to show a brief summary of what the
   * user is about to pay for.
   *
   * e.g. When paying for a DogPermit , the summary could show dog name,
   * licence type, new expiry date, and fee amount.
   *
   * @return array
   *   A render array.
   */
  // public function getPayableItemSummary() {}

  /**
   * Set a CRM contact ID.
   *
   * Individual payable items may use different CRM record fields to associate
   * their contact with.
   *
   * @param string $uuid
   *   A Sugar CRM Contact UUID.
   */
  //public function setContactId(string $uuid);

  /**
   * Get a CRM contact ID.
   *
   * Individual payable items may use different CRM record fields to associate
   * their contact with.
   *
   * @return string
   *   A Sugar CRM Contact UUID.
   *
   * @todo rename this as getPurchaserContactId() ???
   *   To make it clear which interface it is from.
   *   The problem is that when you get an lcc_Permit record from CRM, the owner
   *   UUID doesn't get included in the response.  So PermitInterface will also
   *   need a method to get the contact ID, and we want to disambiguate these
   *   methods.
   */
  //public function getContactId();

}
