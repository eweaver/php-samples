<?php

/**
 * Implement the base object API for Graph 2.0 APIs.  APIs that
 * extend this class represent a single object.
 *
 * @package Graph
 * @subpackage Core
 * @author Eric Weaver
 * @since 2013-10-12
 */
abstract class ObjectApi implements IGraphApi
{
  /**
   * Object identifier
   *
   * @var string|int
   */
  protected $id;

  /**
   * Object identifier, for compatibility.  Is equal to ObjectApi::$id
   *
   * @var string|int
   */
  protected $uuid;

  /**
   * Object reference id.  This may or may not be equal to ObjectApi::$id
   *
   * @var string|int
   */
  protected $referenceId;

  /**
   * Request options (limit, offset, etc).
   *
   * @var array
   */
  protected $options;

  /**
   * Request viewer
   *
   * @var IViewer
   */
  protected $viewer;

  /**
   * Class constructor
   *
   * @param string|int $id
   * @param string|int $referenceId
   * @param array $options
   * @param IViewer|null $viewer
   */
  public final function __construct( $id, $referenceId, array $options = array( ), IViewer $viewer = null )
  {
    $this->id = $this->uuid = $id;
    $this->referenceId = $referenceId;
    $this->options = $options;
    $this->viewer = $viewer;
  }
}
