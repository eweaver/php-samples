<?php

/**
 * Request parser designed to take Facebook Graph API http style requests.
 *
 * Example:
 *
 * http://GRAPH/<object-id>?fields=name,first_name,picture.url(large,square),gender,age
 *
 * @package Libraries
 * @subpackage Facebook
 * @author Eric Weaver
 * @since 2013-10-28
 */
class FacebookGraphRequestParser implements IGraphCoreParser
{
  /**
   * @var array
   */
  private static $validActions = array( IPermission::GET => true, IPermission::POST => true, IPermission::DELETE => true );

  /**
   * Perform any initialization required.
   *
   * @param array $vargs
   * @return void
   */
  public function init( $vargs )
  {
    // Noop
  }

  // IGraphCoreParser

  /**
   * Parse a request entity, returning a GraphRequest.  Additional
   * meta data may be passed in to aid parsing, but is not guaranteed
   * to be provided.
   *
   * @param Context $context
   * @param string $method
   * @param string $requestEntity
   * @param array $optionsData
   * @return GraphRequest
   * @throws RequestParseException
   */
  public function parse( Context $context, $method, $requestEntity, array $optionsData = array( ) )
  {
    $graphRequest = new GraphRequest( $requestEntity );
    $this->parseRequest( $context->getViewer( ), $graphRequest );

    $graphOptions = $this->parseOptions( $context, $method, $requestEntity );
    $graphRequest->setOptions( $graphOptions );

    ObserverDispatch::notify( 'GraphRequest', $graphRequest );

    return $graphRequest;
  }

  // Utility

  /**
   * Parse an incoming request.  Looks for the target UUID or any valid
   * aliases that map to an object.
   *
   * @param IViewer $viewer
   * @param GraphRequest $graphRequest
   * @return GraphRequest
   * @throws RequestParseException
   */
  protected function parseRequest( IViewer $viewer, $graphRequest )
  {
    // We use URL formatting for requests
    list( $request, ) = explode( '?', $graphRequest->entity );

    $parsedRequest = parse_url( $request );
    $path = explode( '/', substr( $parsedRequest['path'], 1 ) );

    $ruleMatched = false;

    // Is UUID
    if ( TypeHelper::isUUID( $path[0] ) === true )
    {
      $ruleMatched = true;
      $graphRequest->uuid = $path[0];

      try
      {
        $objectData = MappingHelper::UUIDtoIdAndObjectType( $graphRequest->uuid );
        $graphRequest->referenceId = $objectData->id;
        $graphRequest->objectStruct = MappingHelper::buildTypeStruct( $objectData->id, $objectData->type );
      }
      catch ( NoObjectDataException $e )
      {
        if ( substr( $graphRequest->uuid, 0, 8 ) === '00000000' )
        {
          $graphRequest->referenceId = TypeHelper::UUIDToInt( $path[0] );
          $graphRequest->objectStruct = MappingHelper::buildTypeStruct( $graphRequest->uuid, 'Picture' );
        }
        else
        {
          $graphRequest->referenceId = $path[0];
          $graphRequest->objectStruct = MappingHelper::buildTypeStruct( $path[0], 'Post' );
        }
      }

      if ( count( $path ) > 1 )
      {
        array_shift( $path );
        $graphRequest->connection = implode( '/', $path );
      }
    }

    // Is numeric (assumes Member Id)
    else if ( is_numeric( $path[0] ) === true )
    {
      $ruleMatched = true;
      $graphRequest->referenceId = $path[0];
      $graphRequest->uuid = MappingHelper::IdAndObjectTypeToUUID( $path[0], 'member' );
      $graphRequest->objectStruct = MappingHelper::buildTypeStruct( $path[0], 'Member' );

      if ( count( $path ) > 1 )
      {
        array_shift( $path );
        $graphRequest->connection = implode( '/', $path );
      }
    }
    // Is me alias
    else if ( $path[0] === GraphRequest::ALIAS_ME )
    {
      $ruleMatched = true;
      $graphRequest->useRequestCache = true;

      if ( ! $viewer instanceof MemberViewer )
      {
        throw new RequestParseException( RequestParseException::CODE_ME );
      }

      $memberId = $viewer->getMemberId( );

      if ( $memberId == MemberViewer::LOGGED_OUT_VIEWER )
      {
        throw new RequestParseException( RequestParseException::CODE_ME );
      }

      $graphRequest->uuid = $memberId;
      $graphRequest->objectStruct = MappingHelper::buildTypeStruct( $memberId, 'Member' );

      if ( count( $path ) > 1 )
      {
        array_shift( $path );
        $graphRequest->connection = implode( '/', $path );
      }
    }

    // Assumed to be a memorable name
    else if ( is_string( $path[0] ) )
    {
      $memberId = MemberMemorableName::getInstance( )->getMemberByMemorableName( $path[0] );

      if ( $memberId !== false )
      {
        $ruleMatched = true;
        $graphRequest->uuid = MappingHelper::IdAndObjectTypeToUUID( $memberId, 'member' );
        $graphRequest->objectStruct = ObjectModel::getObjectStruct( 'Member' );

        if ( count( $path ) > 1 )
        {
          array_shift( $path );
          $graphRequest->connection = implode( '/', $path );
        }
      }
    }

    // If we didn't match any rules, we don't know how to route the request
    if ( $ruleMatched === false )
    {
      throw new RequestParseException( RequestParseException::CODE_INVALID_REQUEST );
    }

    return $graphRequest;
  }

  /**
   * Parse options for the request.  Options are parsed from the query string
   * portion of a request.
   *
   * @param Context $context
   * @param string $method
   * @param string $entity
   * @return GraphOptions
   * @throws RequestParseException
   */
  protected function parseOptions( Context $context, $method, $entity )
  {
    $options = new GraphOptions( );

    // We use URL formatting for requests
    list( , $queryString ) = explode( '?', $entity );

    if ( ! $queryString )
    {
      return $options;
    }

    $queryStringParameters = explode( '&', $queryString );

    if ( count( $queryStringParameters ) > 0 )
    {
      foreach ( $queryStringParameters as $queryStringParameter )
      {
        list( $name, $value ) = explode( '=', $queryStringParameter );

        if ( strlen( $value ) < 1 )
        {
          continue;
        }

        if ( $name === GraphOptions::PARAMETER_LIMIT && is_numeric( $value ) )
        {
          $value = (int) $value;
          $options->limit = ( $value <= GraphOptions::PARAMETER_LIMIT_MAX ? $value : GraphOptions::PARAMETER_LIMIT_MAX );
        }
        else if ( $name === GraphOptions::PARAMETER_OFFSET && is_numeric( $value ) )
        {
          $options->offset = (int) $value;
        }
        else if ( $name === GraphOptions::PARAMETER_METHOD )
        {
          if ( $method !== IPermission::GET )
          {
            throw new RequestParseException( RequestParseException::CODE_CANNOT_OVERRIDE );
          }

          if ( ! isset( self::$validActions[ $value ] ) )
          {
            $message = array( 'method' => $value );
            throw new RequestParseException( RequestParseException::CODE_INVALID_METHOD, $message );
          }

          if ( $context->allowRequestsAsGet( ) === false )
          {
            $message = array( 'method' => $value );
            throw new RequestParseException( RequestParseException::CODE_INVALID_METHOD, $message );
          }

          $options->method = $value;
        }
        else if ( $name === GraphOptions::PARAMETER_FIELDS )
        {
          list( $options->fields, $options->expansions ) = $this->parseFields( $value );
        }
      }
    }

    return $options;
  }

  /**
   * Validate object specific options.
   *
   * @param ObjectModel $objectModel Object Model
   * @param object $objectStruct Object structure representation
   * @param string $entity
   * @throws RequestParseException
   * @return bool
   */
  public function validateOptions( $objectModel, $objectStruct, $entity )
  {
    list( , $queryString ) = explode( '?', $entity );

    if ( ! $queryString )
    {
      return false;
    }

    $queryStringParameters = explode( '&', $queryString );

    if ( count( $queryStringParameters ) > 0 )
    {
      // Object specific parameter checking
      $objectSpecific = array( );

      foreach ( $queryStringParameters as $queryStringParameter )
      {
        list( $name, $value ) = explode( '=', $queryStringParameter );

        if ( strlen( $value ) < 1 )
        {
          continue;
        }

        // The handling of 'fields' is consistent across all object types
        if ( $name === GraphOptions::PARAMETER_FIELDS )
        {
          list( $value ) = $this->parseFields( $value );

          // Check for invalid properties
          $invalidProperties = pBaseDataObject::hasInvalidProperties( $value, $objectModel );

          if ( $invalidProperties !== false )
          {
            throw new RequestParseException( RequestParseException::CODE_INVALID_FIELDS,
              array( GraphOptions::PARAMETER_FIELDS => implode( ',', $invalidProperties ) ) );
          }
        }
        else
        {
          // Default behavior is to let the DataObject handle it
          $objectSpecific[$name] = $value;
        }
      }

      // We have object specific pre-processing to do.
      if ( count( $objectSpecific ) > 0 && in_array( 'IProcessableDataObject', class_implements( $objectStruct->class ) ) )
      {
        // If the pre processing comes back true, set the valid options
        $validOptions = call_user_func( array( $objectStruct->class, 'preProcessing' ), $objectSpecific );

        if ( $validOptions !== false )
        {
          $this->options = array_merge( $this->options, $validOptions );

          // If an API has been set up, pass the new options through.
          if ( $this->api !== NULL )
          {
            $this->api->addOptions( $validOptions );
          }
        }
      }
    }

    return true;
  }

  /**
   * Parse a standard fields string into the fields and expansion operations.
   *
   * fields=name,first_name,last_name,picture.type(large|square),gender,age
   *
   * @param string $fieldsString
   * @return array
   */
  protected function parseFields( $fieldsString )
  {
    $fields = explode( ',', $fieldsString );
    $expansions = array( );

    if ( strpos( $fieldsString, '.' ) !== false )
    {
      $s = -1;
      $directive = null;

      foreach ( $fields as $index => $field )
      {
        if ( strpos( $field, '.' ) !== false )
        {
          list( $fieldName, $expansionData ) = explode( '.', $field );
          $fields[ $index ] = $fieldName;
          list( $directive, $data ) = $this->parseExpansions( $expansionData );
          $expansions[ $fieldName ][ $directive ][] = $data;

          if ( strpos( $field, ')' ) === false )
          {
            $s = $fieldName;
          }
        }
        else if ( $s !== -1 )
        {
          if ( strpos( $field, ')' ) !== false )
          {
            list( , $data ) = $this->parseExpansions( $field );
            $expansions[ $s ][ $directive ][] = $data;
            $s = -1;
            $directive = null;
            unset( $fields[ $index ] );
          }
          else
          {
            $expansions[ $s ][ $directive ][] = $field;
            unset( $fields[ $index ] );
          }
        }
      }
    }

    // TODO: Remove json decode/encode silliness
    return array( $fields, $expansions );
  }

  /**
   * Parse expansion data
   *
   * @param string $expansionData
   * @return array [ directive, data ]
   */
  protected function parseExpansions( $expansionData )
  {
    $directive = null;
    $expansionField = null;

    if ( strpos( $expansionData, '(' ) !== false )
    {
      list( $directive, $expansionField ) = explode( '(', $expansionData );
    }
    else
    {
      $expansionField = $expansionData;
    }

    return array( $directive, trim( $expansionField, ')' ) );
  }
}