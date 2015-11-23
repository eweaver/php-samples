<?php

/**
 * Parser for object annotations
 *
 * @package Graph
 * @subpackage Core
 * @author Eric Weaver
 * @since 2013-09-25
 */
class AnnotationParser
{
  // public
  const ANNOTATION_NAME_AUTHORIZER = 'authorizer';
  const ANNOTATION_NAME_CONNECTION = 'connection';
  const ANNOTATION_NAME_DEFAULT = 'default';
  const ANNOTATION_NAME_MAINTENANCE = 'maintenance';
  const ANNOTATION_NAME_PERMISSIONS = 'permissions';
  const ANNOTATION_NAME_PREPROCESSOR = 'preprocessor';
  const ANNOTATION_NAME_POSTPROCESSOR = 'postprocessor';
  const ANNOTATION_NAME_SETTING = 'setting';
  const ANNOTATION_NAME_TYPE = 'type';
  const ANNOTATION_NAME_VERSION = 'version';

  // Private
  const ANNOTATION_AUTHORIZER = 'AuthorizerAnnotation';
  const ANNOTATION_CONNECTION = 'ConnectionAnnotation';
  const ANNOTATION_DEFAULT = 'DefaultAnnotation';
  const ANNOTATION_MAINTENANCE = 'MaintenanceAnnotation';
  const ANNOTATION_PERMISSIONS = 'PermissionsAnnotation';
  const ANNOTATION_PREPROCESSOR = 'PreprocessorAnnotation';
  const ANNOTATION_POSTPROCESSOR = 'PostprocessorAnnotation';
  const ANNOTATION_SETTING = 'SettingAnnotation';
  const ANNOTATION_TYPE = 'TypeAnnotation';
  const ANNOTATION_VERSION = 'VersionAnnotation';

  private static $allowedAnnotations = array(
    self::ANNOTATION_NAME_AUTHORIZER => self::ANNOTATION_AUTHORIZER,
    self::ANNOTATION_NAME_CONNECTION => self::ANNOTATION_CONNECTION,
    self::ANNOTATION_NAME_DEFAULT => self::ANNOTATION_DEFAULT,
    self::ANNOTATION_NAME_MAINTENANCE => self::ANNOTATION_MAINTENANCE,
    self::ANNOTATION_NAME_PERMISSIONS => self::ANNOTATION_PERMISSIONS,
    self::ANNOTATION_NAME_POSTPROCESSOR => self::ANNOTATION_POSTPROCESSOR,
    self::ANNOTATION_NAME_PREPROCESSOR => self::ANNOTATION_PREPROCESSOR,
    self::ANNOTATION_NAME_SETTING => self::ANNOTATION_SETTING,
    self::ANNOTATION_NAME_TYPE => self::ANNOTATION_TYPE,
    self::ANNOTATION_NAME_VERSION => self::ANNOTATION_VERSION
  );

  private static $parsers = array( );

  /**
   * Parse an annotation.
   *
   * @param string $annotationName
   * @param string $annotationData
   * @param mixed|NULL $metadata
   * @return mixed
   */
  public function parse( $annotationName, $annotationData, $metadata = NULL )
  {
    if ( $this->isValidAnnotation( $annotationName ) === false )
    {
      return NULL;
    }

    if ( ! isset( self::$parsers[ $annotationName ] ) )
    {
      self::$parsers[ $annotationName ] = new self::$allowedAnnotations[ $annotationName ]( );
    }

    return self::$parsers[ $annotationName ]->parse( $annotationData, $metadata );
  }

  /**
   * Annotation config is method specific
   *
   * @return bool
   */
  public function isByMethod( $annotationName )
  {
    if ( $this->isValidAnnotation( $annotationName ) === false )
    {
      return NULL;
    }

    if ( ! isset( self::$parsers[ $annotationName ] ) )
    {
      self::$parsers[ $annotationName ] = new self::$allowedAnnotations[ $annotationName ]( );
    }

    return self::$parsers[ $annotationName ]->isByMethod( );
  }

  /**
   * Allows multiple annotations with the same name
   *
   * @return bool
   */
  public function allowsMultiple( $annotationName )
  {
    if ( $this->isValidAnnotation( $annotationName ) === false )
    {
      return NULL;
    }

    if ( ! isset( self::$parsers[ $annotationName ] ) )
    {
      self::$parsers[ $annotationName ] = new self::$allowedAnnotations[ $annotationName ]( );
    }

    return self::$parsers[ $annotationName ]->allowsMultiple( );
  }

  /**
   * Return if an annotation name is one of the allowed object template
   * annotations.
   *
   * @param string $name
   * @return bool
   */
  public function isValidAnnotation( $name )
  {
    return isset( self::$allowedAnnotations[ $name ] );
  }

  /**
   * Given an array of annotation names, return those that are allowed object
   * template annotations.
   *
   * @param array $annotationNames
   * @return array
   */
  public function getValidAnnotations( array $annotationNames )
  {
    return array_intersect_key( $annotationNames, array_keys( self::$allowedAnnotations ) );
  }
}
