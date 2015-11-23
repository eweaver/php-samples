<?php

/**
 * Abstract router class for interfacing with the Platform API.  Provides
 * common functionality to all routers, including pre/post processing and
 * output support.
 *
 * @package Graph
 * @subpackage Router
 * @author Eric Weaver
 * @since 2010-09-01
 * @version 2.0
 */
abstract class GraphRouter
{
  /**
   * Suffix for generating API class names.
   *
   */
  const API_SUFFIX = 'API';

  /**
   * The request entity.  Contains the original request, the request
   * method, the request data and output.
   *
   * @var object
   */
  protected $request;

  /**
   * Object representing the request context.
   *
   * @var Context
   */
  protected $context;

  /**
   * Context data
   *
   * @var mixed
   */
  protected $contextData;

  /**
   * Request viewer
   *
   * @var IViewer
   */
  protected $viewer;

  /**
   * Target UUID
   *
   * @var string
   */
  protected $uuid;

  /**
   * ReferenceId used with API requests.
   *
   * @var int|UUID
   */
  protected $referenceId;

  /**
   * API to call
   *
   * @var string
   */
  protected $connectionAPI;

  /**
   * Contains object mapping data.
   *
   * @var object
   */
  protected $objectStruct;

  /**
   * Object model for requested object type
   *
   * @var ObjectModel
   */
  protected $objectModel;

  /**
   * Query string parameters.  Default behaviors for various
   * options are set here.
   *
   * @var array
   */
  protected $options = array(
    GraphOptions::PARAMETER_LIMIT => 10,
    GraphOptions::PARAMETER_OFFSET => 0
  );

  /**
   * HTTP Request Method; Supported methods are GET, POST, DELETE
   *
   * @var string
   */
  protected $method;

  /**
   *  Method handler instance to delegate API request work to
   *
   * @var IMethodHandler
   */
  protected $methodHandler;

  /**
   * Request level cache delegate
   *
   * @var RequestCache
   */
  protected $requestCache;

  /**
   * Object connection.
   *
   * @var string
   */
  protected $connection;

  /**
   * Flag to set that a return HTTP status code is needed.
   *
   * TODO: remove from base router
   *
   * @var boolean
   */
  protected $usesHTTPCode = false;

  /**
   * Instance of pBaseAPI
   *
   * @var pBaseAPI
   */
  protected $api;

  /**
   * Reserved properties that will be stripped from incoming data requests.
   *
   * @var array
   */
  protected static $reservedProperties = array( );

  // :MARK Init

  /**
   * Class constructor for router classes.
   */
  public function __construct( )
  {
    ini_set( 'html_errors', false );
    GraphExtensionLoader::init( );
  }

  /**
   * Set up the observers for the request.
   *
   * @return void
   */
  protected final function registerObservers( )
  {
    // Set up dispatcher (false => turns off remoting)
    ObserverDispatch::init( false );

    $observersConfig = new JSONCoreBootstrap( );
    $observers = (array) $observersConfig->get( JSONCoreBootstrap::SIMPLE_JSON_OBSERVERS_KEY );

    foreach ( $observers as $observerClass )
    {
      ObserverDispatch::registerObserver( new $observerClass( ) );
    }
  }

  /**
   * Detach the observers from the request
   *
   * @return void
   */
  protected final function detachObservers( )
  {
    ObserverDispatch::detachObservers( );
  }

  /**
   * Get an array of gatekeepers to apply to the request
   *
   * @return array
   */
  protected final function getGatekeepers( )
  {
    $gatekeeperConfig = new JSONCoreBootstrap( );
    $gateKeepers = array( );
    $gateKeeperClasses = (array) $gatekeeperConfig->get( JSONCoreBootstrap::SIMPLE_JSON_GATEKEEPERS_KEY );

    foreach ( $gateKeeperClasses as $gateKeeperClass )
    {
      $gateKeepers[] = new $gateKeeperClass( );
    }

    return $gateKeepers;
  }

  // :MARK Request flow

  /**
   * Gateway to all Router requests.  Wraps request in a try/catch blocking looking
   * for all possible User Level exceptions.  Calls the appropriate sendOutput( )
   * method when complete.
   *
   * @param mixed $request Request entity
   * @param string $method Method requested
   * @param mixed $data Data for the request
   * @return mixed
   */
  protected final function doRequest( $request, $method = IPermission::GET, $data = false )
  {
    $this->request = new stdClass( );
    $this->request->entity = $request;
    $this->contextData[ ContextKeys::MARKER_REQUEST_DATA ] = $data;
    $this->request->data = $this->removeReservedProperties( $data );
    $this->request->output = false;

    $this->method = $method;

    try
    {
      // Set up observers for the request
      $this->registerObservers( );

      // Set up the context
      $this->context = Context::getInstance( (array) $this->contextData );

      // Set the viewer that was built by the context
      $this->viewer = $this->context->getViewer( );

      // Set up request cache
      $this->requestCache = new RequestCache( $this->context->getRequestId( ) );

      $parser = $this->context->getRequestParser( );
      $graphRequest = $parser->parse( $this->context, $this->method, $this->request->entity, $this->request->data );

      // Apply gatekeepers
      $gateKeepers = $this->getGatekeepers( );

      if ( ! empty( $gateKeepers ) )
      {
        foreach ( $gateKeepers as $gateKeeper )
        {
          if ( $gateKeeper->isValid( $graphRequest ) )
          {
            $graphRequest = $gateKeeper->modifyRequest( $this->viewer, $graphRequest );
          }
        }
      }

      if ( $graphRequest->getOptions( )->hasMethodOverride( ) )
      {
        $this->method = $graphRequest->getOptions( )->method;
      }

      if ( $graphRequest->getOptions( )->output )
      {
        $this->context->setOutputOptions( $graphRequest->getOptions( )->output );
      }

      // Find a better way to set this info
      $this->uuid = $graphRequest->uuid;
      $this->connection = $graphRequest->connection;
      $this->referenceId = $graphRequest->referenceId;
      $this->objectStruct = $graphRequest->objectStruct;
      $this->options = $graphRequest->getOptions( )->toArray( );

      // Process the request and get the return data
      $returnData = $this->processRequest( );
    }
    catch ( Exception $e )
    {
      // If the error was thrown from within context, we can't guarantee we'll have on at this point.
      // Use an error instance context in that case.
      if ( ! $this->context instanceof Context )
      {
        $this->context = Context::getErrorInstance( );
      }

      $returnData = $this->getError( $e );
    }

    return $this->finishRequest( $returnData );
  }

  /**
   * Process the request and delegate it to the appropriate api
   *
   * @return bool|object
   * @throws OperationNotPermittedException
   */
  protected final function processRequest( )
  {
    $returnData = false;

    if ( $this->connection )
    {
      $connectionObject = ObjectModel::getInstance( $this->objectStruct->type )->getConnectionObject( $this->connection );
      $requestHandler = new ConnectionApiHandler( $this->viewer, $this->connection, $connectionObject,
                                                  $this->uuid, $this->referenceId, $this->objectStruct, (array) $this->options );
    }
    else
    {
      $requestHandler = new ObjectApiHandler( $this->viewer, $this->connection, $this->uuid, $this->referenceId, $this->objectStruct, (array) $this->options );
    }

    $this->api = $requestHandler->getApi();
    $objectStruct = $requestHandler->getApiObjectStruct( );

    // Can throw any type of GraphException on an invalid request
    $requestHandler->validateRequest( $objectStruct );

    $parser = $this->context->getRequestParser( );
    $parser->validateOptions( $requestHandler->getApiObjectModel( ), $objectStruct->type, $this->request->entity );

    if ( ClearCacheHandler::isRequestingCacheClear( $this->viewer, $this->contextData ) === true )
    {
      $this->methodHandler = new ClearCacheHandler( $requestHandler, $this->request->entity );
      $this->methodHandler->handleRequest( $objectStruct );
    }
    else
    {
      // Can throw an OperationNotPermittedException on an invalid method
      $handlerClass = AbstractMethodHandler::getHandlerClassName( $this->method );
      $this->methodHandler = new $handlerClass( $requestHandler, $this->request->entity );

      $this->preProcessing( $objectStruct );
      $this->objectData = $this->methodHandler->handleRequest( $this->request->data );

      $returnData = $this->postProcessing( $objectStruct );
    }

    return $returnData;
  }

  /**
   * Finish the request and push any output.
   *
   * @param $returnData
   * @return mixed
   */
  protected final function finishRequest( $returnData )
  {
    UriUtility::rewriteUrls( $returnData, self::PROTOCOL_HTTPS );

    // Add debug content for debug users
    $this->getDebug( $returnData );

    // Reset router state
    $this->resetRouter( );

    switch( $this->method )
    {
      case IPermission::GET:
        $validTypes =  array( 'DataSetResponse', 'ObjectResponse', 'OperationResponse' );
        break;
      case IPermission::POST:
      case IPermission::PUT:
      $validTypes = array( 'ObjectResponse', 'IncompleteResponse', 'OperationResponse' );
        break;
      case IPermission::DELETE:
        $validTypes = array( 'BooleanResponse', 'OperationResponse' );
        break;
    }

    $isValid = false;

    foreach ( $validTypes as $responseType )
    {
      $responseClass = new $responseType( $returnData );

      if ( $responseClass->isValid( $this->method, $returnData ) )
      {
        $isValid = true;
        break;
      }
    }

    if ( $isValid === false )
    {
      throw new GraphRequestException( GraphRequestException::CODE_INVALID_RESPONSE_OBJECT, array( 'method' => $this->method ) );
    }

    return $this->sendOutput( $responseClass, array( ) );
  }

  // :MARK Processors

  /**
   * Apply interaction rules based on the viewer and the requested target.  This
   * method directly modifies pRouter::$validProperties, removing properties that
   * are no longer considered valid because of a rule.
   *
   * @param object $permissionsModel
   * @param object $objectStruct
   * @return void
   */
  protected function applyViewerInteractionRules( $permissionsModel, $objectStruct )
  {
    if ( $this->viewer instanceof MeberViewer )
    {
      $viewerAuthorization = new ViewerInteractionAuthorization( $this->viewer );
      $viewerAuthorization->setTarget( $this->uuid, $permissionsModel, $objectStruct );

      $this->validProperties = $viewerAuthorization->filterProperties( $this->permissionsFlag, $this->method, $this->validProperties );

      $authorizationError = $viewerAuthorization->getAuthorizationError( );

      if ( $authorizationError !== NULL )
      {
        $messageData = array( 'message' => $authorizationError );
        throw new PermissionDeniedException( PermissionDeniedException::CODE_AUTHORIZATION_FAILED, $messageData );
      }
    }

    // At this point, if id was stripped from the available properties, the user shouldn't be able to see *anything*
    if ( ! in_array( pBaseDataObject::ID_LABEL, $this->validProperties ) )
    {
      $messageData = array(
        'object_type' => $objectStruct->type,
        'properties' => pBaseDataObject::ID_LABEL
      );
      throw new PermissionDeniedException( PermissionDeniedException::CODE_INVALID_PROPERTIES, $messageData );
    }
  }

  /**
   * Apply property level pre processing.
   *
   * @param object $objectModel
   * @param object $data
   * @return void
   */
  protected function applyPreProcessors( $objectModel, $data )
  {
    if ( $this->viewer instanceof MemberViewer )
    {
      $processors = (array) $objectModel->getPropertiesMetaData( 'preprocessors' );

      if ( ! empty( $processors ) && isset( $processors[$this->method] ) )
      {
        $processorManager = new ProcessorManager( $objectModel, $this->viewer );
        list( $this->validProperties, $this->request->data )
          = $processorManager->applyProcessorsToArray( $this->method, (array) $processors[$this->method], $this->validProperties, $data );

        $removedProperties = $processorManager->getRemovedProperties( );

        if ( ! empty( $removedProperties ) )
        {
          $message = $processorManager->getRemovedPropertiesMessage( );
          throw new GraphProcessorException( GraphProcessorException::CODE_MULTI_REMOVED_PROPERTIES,
            array( 'properties' => $message ),
            $processorManager->getRemovedPropertyNames( ) );
        }
      }
    }
  }

  /**
   * Apply property level post processing.  Post processors can only modify content,
   * not restrict/remove properties.
   *
   * @param object $objectModel
   * @param object $data
   * @return void
   */
  protected function applyPostProcessors( $objectModel, $data )
  {
    if ( $this->viewer instanceof MemberViewer )
    {
      $processors = (array) $objectModel->getPropertiesMetaData( 'postprocessors' );

      if ( ! empty( $processors ) && isset( $processors[$this->method] ) )
      {
        $processorManager = new ProcessorManager( $objectModel, $this->viewer );
        list( $this->validProperties, $data )
          = $processorManager->applyProcessorsToObject( $this->method, $processors[$this->method], $this->validProperties, $data );
      }
    }
  }

  /**
   * Request pre-processing.  Options parsing and  request validation
   * are done here.
   *
   * @param object $objectStruct Object struct for the object type corresponding to the given UUID.
   * @return void
   * @throws NoObjectDataException
   * @throws PlatformObjectException
   * @throws OperationNotPermittedException
   * @throws PermissionDeniedException
   */
  protected final function preProcessing( $objectStruct )
  {
    list( $objectModel, $permissionsModel ) = $this->getModels( $objectStruct->type );

    if ( $this->methodHandler->isRequestCacheEnabled( ) === true )
    {
      $cacheKey = $this->requestCache->getRequestCacheKey( $this->request->entity, false );
      $authChecks = $this->requestCache->getRequestCache( $cacheKey . ':processing:' . $this->method );

      if ( $authChecks )
      {
        $this->permissionsFlag = $authChecks;
        $this->setRequestProperties( $objectModel, $permissionsModel );
        $this->applyPreProcessors( $objectModel, $this->request->data );
        return;
      }
    }

    $this->permissionsFlag = call_user_func( array( $objectStruct->class, 'check' ), $this->viewer, $this->uuid, $this->method );
    $this->setRequestProperties( $objectModel, $permissionsModel );
    $this->applyViewerInteractionRules( $permissionsModel, $objectStruct );

    // Cache before processors, processors must always be enabled
    if ( $this->methodHandler->isRequestCacheEnabled( ) === true )
    {
      $cacheKey = $this->requestCache->getRequestCacheKey( $this->request->entity, false ) . ':processing:' . $this->method;
      $this->requestCache->setRequestCache( $cacheKey, $this->permissionsFlag, 300 );
    }

    $this->applyPreProcessors( $objectModel, $this->request->data );
  }

  /**
   * Post processing tasks.  This includes applying external permissions and
   * filtering data based on the initial request.
   *
   * @param object $objectStruct Object structure representation
   * @return object
   */
  protected final function postProcessing( $objectStruct )
  {
    list( $objectModel, ) = $this->getModels( $objectStruct->type );
    $returnData = $this->methodHandler->postProcessing( $this->objectData );

    if ( is_object( $returnData ) && $this->method === IPermission::GET )
    {
      $returnData = $this->loadConnectionsAsProperties( $objectModel, $returnData );
    }

    if ( is_bool( $returnData ) === false )
    {
      $objectModelProperties = $objectModel->getProperties( );

      if ( is_object( $returnData ) && ! is_array( $returnData->data ) )
      {
        $returnData = pBaseDataObject::filter( $returnData, $objectModelProperties, $this->validProperties );
      }
    }

    $this->applyPostProcessors( $objectModel, $returnData );

    return $returnData;
  }

  // :MARK Utility method

  /**
   * Return array of requested fields that can be expanded to connections.
   *
   * @param array $connectionProperties
   * @param array $fields
   * @return array
   */
  protected final function getConnectionAsProperties( $connectionProperties, $fields )
  {
    $vProperties = array_intersect( $fields, $connectionProperties );
    $vProperties = array_diff( $this->validProperties, $vProperties );
    return array_intersect( $connectionProperties, $vProperties );
  }

  /**
   * Load Connections as Properties
   *
   * @param ObjectModel $objectModel object model
   * @param object $returnData Response data
   * @return void
   */
  protected final function loadConnectionsAsProperties( $objectModel, $returnData )
  {
    if ( ! $objectModel instanceof ObjectModel || ! is_object( $returnData ) )
    {
      return false;
    }

    /**
     * Taking an array of valid requested properties, array of object model connection metadata
     * and a data object
     */
    $connectionProperties = array_keys( (array) $objectModel->getMetadata( 'connections' ) );

    $connectionProperties = $this->getConnectionAsProperties( $connectionProperties, array_keys( (array) $returnData ) );

    if ( is_array( $connectionProperties ) && count( $connectionProperties ) > 0 )
    {
      $router = new InternalRouter( GraphInputContext::CONTEXT_MARKER );

      foreach ( $connectionProperties as $connectionName )
      {
        $returnData->{$connectionName} = $router->request( '/' . $this->uuid . '/' . $connectionName, IPermission::GET );
      }
    }

    return $returnData;
  }

  /**
   * Set the requested/valid properties lists.  Depends on the method and permissions
   * flag being set
   *
   * @return void
   * @see pRouter::$requestedProperties
   * @see pRouter::$validProperties
   * @uses pRouter::$permissionsFlag
   * @uses pRouter::$method
   */
  protected function setRequestProperties( $objectModel, $permissionsModel )
  {
    $this->requestedProperties = isset( $this->options[GraphOptions::PARAMETER_FIELDS] )
      ? $this->options[GraphOptions::PARAMETER_FIELDS]
      : $objectModel->getDefaults( );

    // make sure id is always there
    if ( ! in_array( pBaseDataObject::ID_LABEL, $this->requestedProperties ) )
    {
      $this->requestedProperties[] = pBaseDataObject::ID_LABEL;
    }

    $this->validProperties = $permissionsModel->getPropertiesByPermissionsFlag( $this->permissionsFlag,
                                                                                $this->requestedProperties,
                                                                                $this->method );
  }

  /**
   * @param string $objectName
   * @return array
   */
  protected function getModels( $objectName )
  {
    $objectModel = ObjectModel::getInstance( $objectName );
    $permissionsModel = PermissionsModel::getInstance( $objectName );

    if ( ! $objectModel || ! $permissionsModel )
    {
      $templateLoader = new ConfigurationTemplateLoader( );
      $templateLoader->loadTemplate( $objectName );
      $objectModel = ! $objectModel ? $templateLoader->getObjectModel( ) : $objectModel;
      $permissionsModel = ! $permissionsModel ? $templateLoader->getPermissionsModel( ) : $permissionsModel;
    }

    return array( $objectModel, $permissionsModel );
  }

  /**
   * Resets the router state after a request is made.
   *
   * @return void
   */
  protected final function resetRouter( )
  {
    unset( $this->objectStruct );
    unset( $this->objectModel );
    unset( $this->connection );
    unset( $this->request );
    unset( $this->uuid );

    $this->options = array(
      GraphOptions::PARAMETER_LIMIT => 10,
      GraphOptions::PARAMETER_OFFSET => 0
    );
  }

  /**
   * Remove reserved properties from the request data
   *
   * @param $data
   * @return array|bool
   */
  protected final function removeReservedProperties( $data )
  {
    if ( $data === false )
    {
      return false;
    }

    return array_diff_key( $data, self::$reservedProperties );
  }

  /**
   * Returns a formatted object containing details about a caught exception.
   * Additional information is included for debug users.
   *
   * @param Exception $exception An exception that has been caught
   * @return object|false
   */
  protected function getError( Exception $exception )
  {
    $environment = GraphExtensionLoader::getExtension( GraphExtension::CORE_ENVIRONMENT );

    if ( $environment->isDevelopment( ) === true
      || $this->viewer instanceof SystemViewer
      || ( $this->viewer instanceof MemberViewer && $this->viewer->isDebugUser( ) ) )
    {
      $errorHandler = new GraphDebugErrorHandler( $this->usesHTTPCode );
    }
    else
    {
      $errorHandler = new GraphStandardErrorHandler( $this->usesHTTPCode );
    }

    return $errorHandler->getError( $exception );
  }

  /**
   * Modifieds the return data object to contain debug information for
   * debug users.
   *
   * @param object|false &$data Return data
   * @return void
   */
  protected function getDebug( & $data )
  {
    if ( $this->viewer instanceof IViewer && $this->viewer->isDebugUser( ) )
    {
      // If the result was false and we want to add debug data, we need to
      // make this an object first
      if ( $data === false )
      {
        $data = new stdClass( );
        $data->data = false;
      }

      $data->debug = GraphExtensionLoader::getExtension( GraphExtension::CORE_DEBUG )->getSystemDebug( );
    }
  }

  /**
   * Send the request output.  The output formatting is determined by implementations
   * of pRouter.  Child classes should always all this before sending out their API.
   *
   * @param IResponseType $data Response data for a platform request
   * @return mixed Determined by Router implementation.
   */
  protected function sendOutput( IResponseType $data, array $metadata = array( ) )
  {
    $this->detachObservers( );
  }
}
