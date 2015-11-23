<?php

/**
 * This class represents the context in which the Platform request is
 * being handled.  The context determines what type of viewer is making
 * the request, as well as what type of output is preferred.
 *
 * @package Graph
 * @subpackage Context
 * @author Eric Weaver
 * @since 2010-08-19
 */
class Context implements IObservable
{
  /**
   * Key for the context instance name value.  This value is injected into
   * the context data before it passed on to the input context.
   *
   * @const
   */
  const CONTEXT_INSTANCE = ContextKeys::MARKER_INSTANCE;

  /**
   * Instance of IGraphCoreInputContext
   *
   * @var IGraphCoreInputContext
   */
  protected $inputContext;

  /**
   * Instance of IGraphCoreOutputContext
   *
   * @var IGraphCoreOutputContext
   */
  protected $outputContext;

  /**
   * @var ContextData
   */
  protected $contextData;

  /**
   * Instance of Context
   *
   * @var Context
   */
  protected static $instance = array( );

  /**
   * Return an instance of Context
   *
   * @param array $contextData
   * @return Context
   * @throws RequestParseException
   */
  public static function getInstance( array $contextData )
  {
    $instanceName = $contextData[ self::CONTEXT_INSTANCE ];

    if ( isset( self::$instance[ $instanceName ] ) )
    {
      return self::$instance[ $instanceName ];
    }

    $contexts = GraphExtensionLoader::getExtension( GraphExtension::CORE_CONTEXT_INPUT );

    foreach ( $contexts as $contextName )
    {
      $contextClass = new $contextName( );

      // Break on the first available context that can handle the request
      if ( $contextClass->isValidContext( $contextData ) === true )
      {
        break;
      }
    }

    if ( ! $contextClass )
    {
      throw new RequestParseException( );
    }

    $contextClass->parseContextData( $contextData );
    $contextDataStruct = ContextData::getInstance( $contextData );
    self::$instance[ $instanceName ] = new Context( $contextClass, $contextDataStruct );

    // Authenticate based on the input context
    self::$instance[ $instanceName ]->inputContext->authenticate( );

    // Update the Observer dispatch with the viewer instance
    ObserverDispatch::setViewer( self::$instance[$instanceName ]->getViewer( ) );
    ObserverDispatch::notify( 'Context', self::$instance[ $instanceName ] );

    return self::$instance[ $instanceName ];
  }

  /**
   * Return an instance of Context for use when an exception is thrown during
   * the Context building process.  Uses the default GraphInputContext to build
   * output.
   *
   * TODO: Try to push exception throwing as far down the Context stack, such that
   * we can at least try to guess the correct input context during the error case.
   *
   * TODO: Try to eliminate the need for this altogether :)
   *
   * @return Context
   */
  public static function getErrorInstance( )
  {
    return new Context( new GraphInputContext(  ), ContextData::getInstance( array( ) ) );
  }

  /**
   * Return an identifier for this request based on the Viewer.
   *
   * @return string
   */
  public function getRequestId( )
  {
    return $this->inputContext->getViewer( )->getId( );
  }

  /**
   * Use the set OutputContext to output the data.
   *
   * @param IResponseType $data
   * @param array $metadata
   * @return mixed
   */
  public function sendOutput( IResponseType $data, array $metadata = array( ) )
  {
    return $this->outputContext->sendOutput( $data, $metadata );
  }

  /**
   * Return if the input context allows POST/DELETEs to be made as GET
   * requests.
   *
   * @return bool
   */
  public function allowRequestsAsGet( )
  {
    return $this->inputContext->allowRequestsAsGet( );
  }

  /**
   * Set the output context options
   *
   * @param array $options
   * @return void
   */
  public function setOutputOptions( array $options )
  {
    $this->outputContext->setOutputOptions( $options );
  }

  /**
   * Get the IViewer for the current input context.
   *
   * @return IViewer
   */
  public function getViewer( )
  {
    return $this->inputContext->getViewer( );
  }

  /**
   * Return the request parser for the request.
   *
   * @return IGraphCoreParser
   */
  public function getRequestParser( )
  {
    return $this->inputContext->getRequestParser( );
  }

  /**
   * Get a request level cache result
   *
   * @return mixed
   */
  public final function getCachedRequest( $cacheKey )
  {
    $cacheKey = md5( $cacheKey . $this->getRequestId() );
    return CacheHelper::getCache( )->get( $cacheKey );
  }

  /**
   * Set a request level cache result
   *
   * @param $returnData
   */
  public final function setCacheRequest( $cacheKey, $returnData, $timeout )
  {
    $cacheKey = md5( $cacheKey . $this->getRequestId() );
    CacheHelper::getCache( )->set( $cacheKey, $returnData, $timeout );
  }

  /**
   * Class constructor
   *
   * @param IGraphCoreInputContext $inputContext
   * @param ContextData $contextData
   */
  private function __construct( IGraphCoreInputContext $inputContext, ContextData $contextData )
  {
    $this->inputContext = $inputContext;
    $this->outputContext = $inputContext->createOutputContext( );
    $this->contextData = $contextData;
  }

  // IObservable

  /**
   * Return the name of the observable withing the ObserverDispatch system.
   *
   * @return string
   */
  public function getName( )
  {
    return get_class( $this );
  }

  /**
   * Called via ObserverDispatch::notify() to get any data the observable
   * returns for a given tag.
   *
   * @return mixed
   * @throws GraphObserverException Tag does not exist
   * @see ObserverDispatch::notify
   */
  public function getData( )
  {
    $context = new stdClass;
    $viewer = $this->getViewer( );
    $context->viewer = get_class( $viewer );

    if ( $viewer instanceof MemberViewer )
    {
      $context->viewerId = $viewer->getMemberId( );
    }

    $context->inputContext = get_class( $this->inputContext );
    $context->outputContext = get_class( $this->outputContext );
    $context->parser = get_class( $this->getRequestParser( ) );
    return $context;
  }

  /**
   * Call by ObserverDispatch::notifyStateChange() to get the difference in
   * an observable's state before and after a change.
   *
   * @return array( mixed, mixed )
   * @throws GraphObserverException Tag does not exist or no state change
   * @see ObserverDispatch::notifyStateChanged
   */
  public function getStateChange( )
  {
    throw new GraphObserverException( );
  }
}
