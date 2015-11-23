<?php

/**
 * The data representation of an object template.  This class is used
 * by data sources to format data into the object template format.
 *
 *  - Uses ObjectModel to validate incoming data, both data objects and
 *    data sets.
 *  - Provides transparent access to data objects vs. data sets.
 *
 * @package Graph
 * @subpackage Core
 * @author Eric Weaver
 * @since 2013-10-12
 */
class GraphDataObject
{
  const DATA_SET = 'data';
  const PREPEND = -1;
  const APPEND = -2;
  const FIRST_ELEMENT = 0;
  const LAST_ELEMENT = -3;

  /**
   * Object type
   *
   * @var string
   */
  private $objectType;

  /**
   * Instance of ObjectModel for the given object type
   *
   * @var ObjectModel
   */
  private $objectModel;

  /**
   * Represents one or more objects of type GraphObjectData::$objectType using
   * the data source's native properties.
   *
   * @var object|array
   */
  private $dataSourceData;

  /**
   * Represents one or more objects of GraphObjectData::$objectType using it's
   * native properties.
   *
   * @var object|array
   */
  private $objectData;

  /**
   * Strict mode flag.
   *
   * @var bool
   */
  private $isStrictMode;

  /**
   * Is the encapsulated object data representing a set of objects
   *
   * @var bool
   */
  private $isDataSet;

  /**
   * Returns if the encapsulated object data represents a complete view
   * of the object type.  It is considered complete if all data not
   * explicitly listed as @optional are available.
   *
   * For data sets, all objects must be complete.
   *
   *
   * TODO: For now, always assuming complete, this should default to false eventually.
   *
   * @var bool
   */
  private $isComplete = true;

  /**
   * Has a property map been set for returning data in the object type
   * format.
   *
   * @var bool
   */
  private $hasPropertyNameMap = false;

  /**
   * Array containing object type => data source property mappings.
   *
   * @var array
   */
  private $propertyNameMap;

  /**
   * Predefined position markers for use with GraphDataObject::addData
   *
   * @var array
   */
  private static $definedPositions = array(
    self::FIRST_ELEMENT => self::FIRST_ELEMENT,
    self::LAST_ELEMENT => self::LAST_ELEMENT,
    self::APPEND => self::APPEND,
    self::PREPEND => self::PREPEND
  );

  /**
   * Class constructor
   *
   * @param string $objectType
   * @param bool $strictMode
   */
  public function __construct( $objectType, $strictMode = false )
  {
    $this->objectType = $objectType;
    $this->isStrictMode = (bool) $strictMode;
    $this->loadObjectModel( $objectType );
  }

  /**
   * Initialize the GraphObjectData's data.  Setting $isDataSet to
   * true modifies the return type to an array, and allows additional
   * data to be added via GraphObjectData::addData.
   *
   * Data set to the GraphDataObject is immutable.
   *
   * @param object|array $data
   * @param $isDataSet
   * @throws NoObjectDataException
   * @return void
   */
  public function setData( $data, $isDataSet )
  {
    if ( $isDataSet === false && ! is_object( $data ) )
    {
      throw new NoObjectDataException( NoObjectDataException::CODE_INVALID_GRAPH_DATA );
    }
    else if ( $isDataSet === true && ! is_array( $data ) )
    {
      throw new NoObjectDataException( NoObjectDataException::CODE_INVALID_GRAPH_DATA_SET );
    }

    $this->isDataSet = (bool) $isDataSet;
    $this->dataSourceData = $data;
  }

  /**
   * Add data to a data set.  This method requires that the GraphDataObject::isDataSet
   * flag is set to true.  $position can be used to add the data at a specific index. If
   * the index is out of bounds, it will be appended.
   *
   * The constants GraphDataObject::FIRST_ELEMENT, GraphDataObject::LAST_ELEMENT,
   * GraphDataObject::APPEND and GraphDataObject::PREPEND are shortcuts for positioning
   * new data.
   *
   * Data add to the GraphDataObject data set is immutable.
   *
   * @param object $data
   * @param int $position
   * @throws NoObjectDataException
   * @return void
   */
  public function addData( $data, $position = self::APPEND )
  {
    if ( ! is_object( $data ) )
    {
      throw new NoObjectDataException( NoObjectDataException::CODE_INVALID_GRAPH_DATA );
    }

    if ( ! isset( self::$definedPositions[ $position ] ) )
    {
      if ( $position > count( $this->objectData ) - 1 )
      {
        $position = self::LAST_ELEMENT;
      }
      else if ( $position < 1 )
      {
        $position = self::FIRST_ELEMENT;
      }
    }

    switch( $position )
    {
      case self::APPEND:
      case self::LAST_ELEMENT:
        array_push( $this->dataSourceData, $data );
        break;
      case self::PREPEND:
      case self::FIRST_ELEMENT:
        array_unshift( $this->dataSourceData, $data );
        break;
      default:
        $a1 = array_slice( $this->dataSourceData, 0, $position );
        $a2 = array_slice( $this->dataSourceData, $position + 1 );
        $this->dataSourceData = $a1 + array( $data ) + $a2;
        break;
    }
  }

  /**
   * Return an array of properties on the object type, excluding those
   * marked as @private (such as id).
   *
   * @return array
   */
  public function getPropertyNames( )
  {
    $properties = $this->objectModel->getProperties( );

    // Strip out @private properties

    return $properties;
  }

  /**
   * Return the version of the object type that was loaded.
   *
   * @return int
   */
  public function getVersion( )
  {
    return $this->objectModel->getVersion( );
  }

  /**
   * Set the property name map to be used when retrieving the data.  If
   * strict mode is enabled, the property name map will only be accepted if
   * it defines all properties not explicitly marked @optional.
   *
   * @param array $propertyNameMap
   * @return bool
   */
  public function setPropertyNameMap( array $propertyNameMap )
  {
    $this->propertyNameMap = $propertyNameMap;
    $this->hasPropertyNameMap = true;
    return true;
  }

  /**
   * Return the encapsulated data, formatted as the object type.
   *
   * @return object|array
   * @throws NoObjectDataException
   */
  public function getObjectData( )
  {
    if ( isset( $this->objectData ) )
    {
      return $this->objectData;
    }

    if ( $this->hasPropertyNameMap === false )
    {
      $message = array( 'type' => $this->objectType );
      throw new NoObjectDataException( NoObjectDataException::CODE_INVALID_GRAPH_DATA_FORMAT, $message );
    }

    $this->objectData = $this->mapObjectData( $this->dataSourceData );
    return $this->objectData;
  }

  /**
   * Returns if data validation is set to strict mode.  In strict mode
   * all object properties must strictly match the type listed in the
   * object type template.
   *
   * @return bool
   */
  public function isStrictMode( )
  {
    return $this->isStrictMode;
  }

  /**
   * Return if the data is a data set (array), or represents a single
   * object.
   *
   * @return bool
   */
  public function isDataSet( )
  {
    return $this->isDataSet;
  }

  /**
   * Returns if the encapsulated object data represents a complete view
   * of the object type.  It is considered complete if all data not
   * explicitly listed as @optional are available.
   *
   * For data sets, all objects must be complete.
   *
   * @return bool
   */
  public function isComplete( )
  {
    return $this->isComplete;
  }

  /**
   * Return if a mapping between object type and data source property names
   * has been loaded.
   *
   * @return bool
   */
  public function hasPropertyNameMap( )
  {
    return $this->hasPropertyNameMap;
  }

  /**
   * Format data from data source properties => object type properties, using
   * the mapping array set in GraphDataObject::$propertyNameMap
   *
   * @param object|array $data
   * @return object
   */
  private function mapObjectData( $data )
  {
    if ( $this->isComplete === true )
    {
      if ( $this->isDataSet === true )
      {
        $objectData = $this->mapObjectDataSet( $data );
      }
      else
      {
        $objectData = $this->mapObjectDataItem( $data );
      }
    }
    else
    {
      $objectData = $this->mapIncompleteObjectData( $data );
    }

    return $objectData;
  }

  /**
   * Map an incomplete data object.  Incomplete data objects are often
   * using in conjunction with asynchronous data sources.  The incomplete
   * object must contain an id, at a minimum.  Other information provided
   * with the incomplete object are used to retrieve the missing data, or
   * information on when/how the information will be delivered.
   *
   * @param object|array $data
   * @return object
   * @throws NoObjectDataException
   */
  private function mapIncompleteObjectData( $data )
  {
    if ( ! isset( $this->propertyNameMap[ 'id' ] ) || ! isset( $this->propertyNameMap[ '__incomplete'] ) )
    {
      $message = array( 'type' => $this->objectType );
      throw new NoObjectDataException( NoObjectDataException::CODE_INVALID_GRAPH_DATA_FORMAT, $message );
    }

    $objectData = new stdClass;
    $objectData->id = $data->{$this->propertyNameMap[ 'id' ]};
    $objectData->__incomplete = $data->{$this->propertyNameMap[ '__incomplete' ]};

    return $objectData;
  }

  /**
   * Format a single data item.
   *
   * @param object $data
   * @return object
   */
  private function mapObjectDataItem( $data )
  {
    $objectData = new stdClass;

    foreach ( $this->propertyNameMap as $graphObjectName => $dataSourceName )
    {
      $objectData->{$graphObjectName} = $data->{$dataSourceName};
    }

    return $objectData;
  }

  /**
   * Format a data set.
   *
   * @param array $data
   * @return array
   * @throws NoObjectDataException
   */
  private function mapObjectDataSet( array $data )
  {
    $objectData = (object) array( self::DATA_SET => array( ) );

    foreach ( $data as $dataItem )
    {
      if ( ! is_object( $dataItem ) )
      {
        throw new NoObjectDataException( NoObjectDataException::CODE_INVALID_GRAPH_DATA );
      }

      $objectData->data[] = $this->mapObjectDataItem( $dataItem );
    }

    return $objectData;
  }

  /**
   * Load an instance of ObjectModel for the object type
   *
   * @param string $objectType
   * @return void
   */
  private function loadObjectModel( $objectType )
  {
    $this->objectModel = ObjectModel::getInstance( $objectType );
  }
}
