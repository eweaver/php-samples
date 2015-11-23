<?php

/**
 * Extends ObjectApi, providing standard no-op functionality to Verison 2 APIs.
 *
 * @package Version2APIs
 * @author Eric Weaver
 * @since 2013-10-30
 */
abstract class BaseV2API extends ObjectApi
{
  /**
   * Global config, usable by all APIs
   *
   * @var IGraphCoreConfiguration
   */
  private static $config;

  /**
   * Handle a GET request.
   *
   * @param array $params Request parameters
   * @throws OperationNotPermittedException
   * @return object
   */
  public function doGet( array $params )
  {
    $dataRequest = $this->createDataRequest( $this->getObjectType( ) );
    $graphDataObject = $dataRequest->executeRead( $this->referenceId, $params );
    return $graphDataObject->getObjectData( );
  }

  /**
   * Handle a POST request.
   *
   * @param array $params
   * @param mixed $data data to post
   * @throws OperationNotPermittedException
   * @return object
   */
  public function doPost( array $params, array $data )
  {
    throw new OperationNotPermittedException( );
  }

  /**
   * Write (create) data
   *
   * @param array $params
   * @param array $data
   * @return object|array
   */
  public function doPut( array $params, array $data )
  {
    throw new OperationNotPermittedException( );
  }

  /**
   * Handle a DELETE request.
   *
   * @param array $params
   * @throws OperationNotPermittedException
   * @return object
   */
  public function doDelete( array $params )
  {
    throw new OperationNotPermittedException( );
  }

  /**
   * Retrieve cached data for the connection API
   *
   * @return object
   */
  public function getObjectCache( )
  {
    return GraphObject::getObject( $this->getObjectType( ), $this->referenceId );
  }

  /**
   * Prime API cache keys.  This method should be implemented as a stripped down
   * version of IGraphApi::doGet.  This method is not responsible for returning
   * data (nor will it), and can bypass security checks that a normal doGet call
   * would be subject to.
   *
   * @return bool
   */
  public function setObjectCache( )
  {
    return false;
  }

  /**
   * Clear API cache keys and object-level cache keys as necessary.
   *
   * @return bool
   */
  public function deleteObjectCache( )
  {

  }

  /**
   * Return API metadata, including object-level metadata for
   * all objects used by the API
   *
   * @return object
   */
  public function getMetadata( )
  {
    return new stdClass;
  }

  /**
   * Provide API-specific validation of return data.
   *
   * @param mixed $data
   * @return void
   * @throws GraphRequestException
   */
  public function validateReturnData( $data )
  {

  }

  // Helpers

  /**
   * Return a DataRequest
   *
   * @param string $object
   * @return DataRequest
   */
  protected function createDataRequest( $object )
  {
    $dataRequest = new DataRequest( $object, true );
    $dataRequest->loadDataSources( $this->getDataSourceConfig( ) );

    $contextData = ContextData::getInstance( );
    $hintAvailable = $contextData->isHintAvailable( ContextKeys::HINT_REMOTE_TOGGLE );
    $allowRemote = ! $hintAvailable
                     || ( $hintAvailable && $contextData->doesHintEqual( ContextKeys::HINT_REMOTE_TOGGLE, ContextKeys::TOGGLE_ENABLE ) );

    $dataRequest->setProxy( new GraphCacheDataGateway( $this->id, $this->referenceId ), $allowRemote, true );
    return $dataRequest;
  }

  /**
   * Load Version 2 API configuration
   *
   * @return IGraphCoreConfiguration
   */
  protected function getDataSourceConfig( )
  {
    static $config = null;

    if ( $config === null )
    {
      $parameters = array( ObjectDataSourceManager::CONFIG_NAME, GraphConstants::LIB_DIRECTORY . 'Version2APIs/Config/datasources.json' );
      $config = GraphExtensionLoader::getUserExtension( 'JSONConfig', $parameters );
    }

    return $config;
  }
}