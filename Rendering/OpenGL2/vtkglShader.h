/*=========================================================================

  Program:   Visualization Toolkit

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#ifndef __vtkglShader_h
#define __vtkglShader_h

#include "vtkRenderingOpenGL2Module.h"

#include <string> // For member variables.

namespace vtkgl {

/**
 * @brief Vertex or Fragment shader, combined into a ShaderProgram.
 *
 * This class creates a Vertex, Fragment or Geometry shader, that can be attached to a
 * ShaderProgram in order to render geometry etc.
 */

class VTKRENDERINGOPENGL2_EXPORT Shader
{
public:
  /** Available shader types. */
  enum Type {
    Vertex,    /**< Vertex shader */
    Fragment,  /**< Fragment shader */
    Geometry,  /**< Geometry shader */
    Unknown    /**< Unknown (default) */
  };

  explicit Shader(Type type = Unknown, const std::string &source = "");
  ~Shader();

  /** Set the shader type. */
  void SetType(Type type);

  /** Get the shader type, typically Vertex or Fragment. */
  Type GetType() const { return ShaderType; }

  /** Set the shader source to the supplied string. */
  void SetSource(const std::string &source);

  /** Get the source for the shader. */
  std::string GetSource() const { return Source; }

  /** Get the error message (empty if none) for the shader. */
  std::string GetError() const { return Error; }

  /** Get the handle of the shader. */
  int GetHandle() const { return Handle; }

  /** Compile the shader.
   * @note A valid context must to current in order to compile the shader.
   */
  bool Compile();

  /** Delete the shader.
   * @note This should only be done once the ShaderProgram is done with the
   * Shader.
   */
  void Cleanup();

protected:
  Type ShaderType;
  int  Handle;
  bool Dirty;

  std::string Source;
  std::string Error;
};

}

#endif
