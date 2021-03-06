/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkTextureObject.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkTextureObject.h"

#include "vtk_glew.h"

#include "vtkObjectFactory.h"


#if GL_ES_VERSION_2_0 != 1
#include "vtkPixelBufferObject.h"
#endif

#include "vtkOpenGLRenderWindow.h"

#include "vtkTexturedActor2D.h"
#include "vtkNew.h"
#include "vtkPolyDataMapper2D.h"
#include "vtkTexture.h"
#include "vtkDataArray.h"
#include "vtkPoints.h"
#include "vtkPolyData.h"
#include "vtkCellArray.h"
#include "vtkTrivialProducer.h"
#include "vtkFloatArray.h"
#include "vtkRenderer.h"
#include "vtkPointData.h"
#include "vtkOpenGLTexture.h"


#include "vtkOpenGLError.h"

#include <cassert>

//#define VTK_TO_DEBUG
//#define VTK_TO_TIMING

#ifdef VTK_TO_TIMING
#include "vtkTimerLog.h"
#endif

#define BUFFER_OFFSET(i) (static_cast<char *>(NULL) + (i))

// Mapping from DepthTextureCompareFunction values to OpenGL values.

static GLint OpenGLDepthTextureCompareFunction[8]=
{
  GL_LEQUAL,
  GL_GEQUAL,
  GL_LESS,
  GL_GREATER,
  GL_EQUAL,
  GL_NOTEQUAL,
  GL_ALWAYS,
  GL_NEVER
};

static const char *DepthTextureCompareFunctionAsString[8]=
{
  "Lequal",
  "Gequal",
  "Less",
  "Greater",
  "Equal",
  "NotEqual",
  "AlwaysTrue",
  "Never"
};

// Mapping from DepthTextureMode values to OpenGL values.

static GLint OpenGLDepthTextureMode[2]=
{
  GL_LUMINANCE,
  GL_ALPHA
};

static const char *DepthTextureModeAsString[2]=
{
  "Luminance",
  "Alpha"
};

// Mapping from Wrap values to OpenGL values.
static GLint OpenGLWrap[3]=
{
  GL_CLAMP_TO_EDGE,
  GL_REPEAT,
  GL_MIRRORED_REPEAT
};

static const char *WrapAsString[3]=
{
  "ClampToEdge",
  "Repeat",
  "MirroredRepeat"
};

// Mapping MinificationFilter values to OpenGL values.
static GLint OpenGLMinFilter[6]=
{
  GL_NEAREST,
  GL_LINEAR,
  GL_NEAREST_MIPMAP_NEAREST,
  GL_NEAREST_MIPMAP_LINEAR,
  GL_LINEAR_MIPMAP_NEAREST,
  GL_LINEAR_MIPMAP_LINEAR
};

// Mapping MagnificationFilter values to OpenGL values.
static GLint OpenGLMagFilter[6]=
{
  GL_NEAREST,
  GL_LINEAR
};

static const char *MinMagFilterAsString[6]=
{
  "Nearest",
  "Linear",
  "NearestMipmapNearest",
  "NearestMipmapLinear",
  "LinearMipmapNearest",
  "LinearMipmapLinear"
};

static GLenum OpenGLDepthInternalFormat[5]=
{
  GL_DEPTH_COMPONENT,
  GL_DEPTH_COMPONENT16,
#ifdef GL_DEPTH_COMPONENT24
  GL_DEPTH_COMPONENT24,
#else
  GL_DEPTH_COMPONENT16,
#endif
#ifdef GL_DEPTH_COMPONENT32
  GL_DEPTH_COMPONENT32,
#else
  GL_DEPTH_COMPONENT16,
#endif
#ifdef GL_DEPTH_COMPONENT32F
  GL_DEPTH_COMPONENT32F
#else
  GL_DEPTH_COMPONENT16
#endif
};

static GLenum OpenGLDepthInternalFormatType[5]=
{
  GL_UNSIGNED_INT,
  GL_UNSIGNED_INT,
  GL_UNSIGNED_INT,
  GL_UNSIGNED_INT,
#ifdef GL_DEPTH_COMPONENT32F
  GL_FLOAT
#else
  GL_UNSIGNED_INT
#endif
};

/*
static const char *DepthInternalFormatFilterAsString[6]=
{
  "Native",
  "Fixed16",
  "Fixed24",
  "Fixed32",
  "Float32"
};
*/

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkTextureObject);

//----------------------------------------------------------------------------
vtkTextureObject::vtkTextureObject()
{
  this->Context = NULL;
  this->Handle = 0;
  this->NumberOfDimensions = 0;
  this->Target = 0;
  this->Format = 0;
  this->Type = 0;
  this->Components = 0;
  this->Width = 0;
  this->Height = 0;
  this->Depth = 0;
  this->RequireTextureInteger = false;
  this->SupportsTextureInteger = false;
  this->RequireTextureFloat = false;
  this->SupportsTextureFloat = false;
  this->RequireDepthBufferFloat = false;
  this->SupportsDepthBufferFloat = false;
  this->AutoParameters = 1;
  this->WrapS = Repeat;
  this->WrapT = Repeat;
  this->WrapR = Repeat;
  this->MinificationFilter = Nearest;
  this->MagnificationFilter = Nearest;
  this->LinearMagnification = false;
  this->Priority = 1.0f;
  this->MinLOD = -1000.0f;
  this->MaxLOD = 1000.0f;
  this->BaseLevel = 0;
  this->MaxLevel = 0;
  this->DepthTextureCompare = false;
  this->DepthTextureCompareFunction = Lequal;
  this->DepthTextureMode = Luminance;
  this->GenerateMipmap = false;

  this->DrawPixelsActor = NULL;
}

//----------------------------------------------------------------------------
vtkTextureObject::~vtkTextureObject()
{
  if(this->DrawPixelsActor!=0)
    {
    this->DrawPixelsActor->UnRegister(this);
    this->DrawPixelsActor = NULL;
    }
  this->DestroyTexture();
}

//----------------------------------------------------------------------------
bool vtkTextureObject::IsSupported(vtkOpenGLRenderWindow* vtkNotUsed(win),
      bool requireTexFloat,
      bool requireDepthFloat,
      bool requireTexInt)
{
#if GL_ES_VERSION_2_0 != 1
  bool texFloat = true;
  if (requireTexFloat)
    {
    texFloat = (glewIsSupported("GL_ARB_texture_float") != 0);
    }

  bool depthFloat = true;
  if (requireDepthFloat)
    {
    depthFloat = (glewIsSupported("GL_ARB_depth_buffer_float") != 0);
    }

  bool texInt = true;
  if (requireTexInt)
    {
    texInt = (glewIsSupported("GL_EXT_texture_integer") != 0);
    }

#else
  bool texFloat = !requireTexFloat;
  bool depthFloat = !requireDepthFloat;
  bool texInt = !requireTexInt;
#endif

  return texFloat && depthFloat && texInt;
}

//----------------------------------------------------------------------------
bool vtkTextureObject::LoadRequiredExtensions(vtkOpenGLRenderWindow *renWin)
{
  return this->IsSupported(renWin,
    this->RequireTextureFloat,
    this->RequireDepthBufferFloat,
    this->RequireTextureInteger);
}

//----------------------------------------------------------------------------
void vtkTextureObject::SetContext(vtkOpenGLRenderWindow* renWin)
{
  // avoid pointless reassignment
  if (this->Context == renWin)
    {
    return;
    }
  // free previous resources
  this->DestroyTexture();
  this->Context = NULL;
  this->Modified();
  // all done if assigned null
  if (!renWin)
    {
    return;
    }

  if (!this->LoadRequiredExtensions(renWin) )
    {
    vtkErrorMacro("Required OpenGL extensions not supported by the context.");
    return;
    }
  // initialize
  this->Context = renWin;
  this->Context->MakeCurrent();
}

//----------------------------------------------------------------------------
vtkOpenGLRenderWindow* vtkTextureObject::GetContext()
{
  return this->Context;
}

//----------------------------------------------------------------------------
void vtkTextureObject::DestroyTexture()
{
  // deactivate it first
  this->Deactivate();

  // because we don't hold a reference to the render
  // context we don't have any control on when it is
  // destroyed. In fact it may be destroyed before
  // we are(eg smart pointers), in which case we should
  // do nothing.
  if (this->Context && this->Handle)
    {
    GLuint tex = this->Handle;
    glDeleteTextures(1, &tex);
    vtkOpenGLCheckErrorMacro("failed at glDeleteTexture");
    }
  this->Handle = 0;
  this->NumberOfDimensions = 0;
  this->Target =0;
  this->Format = 0;
  this->Type = 0;
  this->Components = 0;
  this->Width = this->Height = this->Depth = 0;
}

//----------------------------------------------------------------------------
void vtkTextureObject::CreateTexture()
{
  assert(this->Context);

  // reuse the existing handle if we have one
  if (!this->Handle)
    {
    GLuint tex=0;
    glGenTextures(1, &tex);
    vtkOpenGLCheckErrorMacro("failed at glGenTextures");
    this->Handle=tex;

    if (this->Target)
      {
      glBindTexture(this->Target, this->Handle);
      vtkOpenGLCheckErrorMacro("failed at glBindTexture");

      // See: http://www.opengl.org/wiki/Common_Mistakes#Creating_a_complete_texture
      // turn off mip map filter or set the base and max level correctly. here
      // both are done.
      glTexParameteri(this->Target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(this->Target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

      glTexParameteri(this->Target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(this->Target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#ifdef GL_TEXTURE_BASE_LEVEL
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
#endif

#ifdef GL_TEXTURE_MAX_LEVEL
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
#endif

      glBindTexture(this->Target, 0);
      }
    }
}

int vtkTextureObject::GetTextureUnit()
{
  return this->Context->GetTextureUnitForTexture(this);
}

//---------------------------------------------------------------------------
void vtkTextureObject::Activate()
{
  // activate a free texture unit for this texture
  this->Context->ActivateTexture(this);
  this->Bind();
}

//---------------------------------------------------------------------------
void vtkTextureObject::Deactivate()
{
  if (this->Context)
    {
    this->Context->ActivateTexture(this);
    this->UnBind();
    this->Context->DeactivateTexture(this);
    }
}

//----------------------------------------------------------------------------
void vtkTextureObject::Bind()
{
  assert(this->Context);
  assert(this->Handle);

  glBindTexture(this->Target, this->Handle);
  vtkOpenGLCheckErrorMacro("failed at glBindTexture");

  if (this->AutoParameters && (this->GetMTime() > this->SendParametersTime))
    {
    this->SendParameters();
    }
}

//----------------------------------------------------------------------------
void vtkTextureObject::UnBind()
{
  glBindTexture(this->Target, 0);
  vtkOpenGLCheckErrorMacro("failed at glBindTexture(0)");
}

//----------------------------------------------------------------------------
bool vtkTextureObject::IsBound()
{
  bool result=false;
  if(this->Context && this->Handle)
    {
    GLenum target=0; // to avoid warnings.
    switch(this->Target)
      {
#if defined(GL_TEXTURE_1D) && defined(GL_TEXTURE_BINDING_1D)
      case GL_TEXTURE_1D:
        target=GL_TEXTURE_BINDING_1D;
        break;
#endif
      case GL_TEXTURE_2D:
        target=GL_TEXTURE_BINDING_2D;
        break;
#if defined(GL_TEXTURE_3D) && defined(GL_TEXTURE_BINDING_3D)
      case GL_TEXTURE_3D:
        target=GL_TEXTURE_BINDING_3D;
        break;
#endif
      default:
        assert("check: impossible case" && 0);
        break;
      }
    GLint objectId;
    glGetIntegerv(target,&objectId);
    result=static_cast<GLuint>(objectId)==this->Handle;
    }
  return result;
}

//----------------------------------------------------------------------------
void vtkTextureObject::SendParameters()
{
  assert("pre: is_bound" && this->IsBound());

  glTexParameteri(this->Target,GL_TEXTURE_WRAP_S, OpenGLWrap[this->WrapS]);
  glTexParameteri(this->Target,GL_TEXTURE_WRAP_T,OpenGLWrap[this->WrapT]);

#ifdef GL_TEXTURE_WRAP_R
  glTexParameteri(
        this->Target,
        GL_TEXTURE_WRAP_R,
        OpenGLWrap[this->WrapR]);
#endif

  glTexParameteri(
        this->Target,
        GL_TEXTURE_MIN_FILTER,
        OpenGLMinFilter[this->MinificationFilter]);

  glTexParameteri(
        this->Target,
        GL_TEXTURE_MAG_FILTER,
        OpenGLMagFilter[this->MagnificationFilter]);


#if GL_ES_VERSION_2_0 != 1
  glTexParameterf(this->Target,GL_TEXTURE_PRIORITY,this->Priority);
  glTexParameterf(this->Target,GL_TEXTURE_MIN_LOD,this->MinLOD);
  glTexParameterf(this->Target,GL_TEXTURE_MAX_LOD,this->MaxLOD);
  glTexParameteri(this->Target,GL_TEXTURE_BASE_LEVEL,this->BaseLevel);
  glTexParameteri(this->Target,GL_TEXTURE_MAX_LEVEL,this->MaxLevel);

  glTexParameteri(
        this->Target,
        GL_DEPTH_TEXTURE_MODE,
        OpenGLDepthTextureMode[this->DepthTextureMode]);

  if(DepthTextureCompare)
    {
    glTexParameteri(
          this->Target,
          GL_TEXTURE_COMPARE_MODE,
          GL_COMPARE_R_TO_TEXTURE);
    }
  else
    {
    glTexParameteri(
          this->Target,
          GL_TEXTURE_COMPARE_MODE,
          GL_NONE);
    }

  glTexParameteri(
        this->Target,
        GL_TEXTURE_COMPARE_FUNC,
        OpenGLDepthTextureCompareFunction[this->DepthTextureCompareFunction]);
#endif

  vtkOpenGLCheckErrorMacro("failed after SendParameters");
  this->SendParametersTime.Modified();
}


#if GL_ES_VERSION_2_0 != 1

//----------------------------------------------------------------------------
unsigned int vtkTextureObject::GetInternalFormat(int vtktype, int numComps,
                                                 bool shaderSupportsTextureInt)
{

  // 1 or 2 components not supported as render target in FBO on GeForce<8
  // force internal format component to be 3 or 4, even if client format is 1
  // or 2 components.
  // see spec 2.1 page 137 (pdf page 151) in section 3.6.4 Rasterization of
  // Pixel Rectangles: "Conversion to RGB": this step is applied only if
  // the format is LUMINANCE or LUMINANCE_ALPHA:
  // L: R=L, G=L, B=L
  // LA: R=L, G=L, B=L, A=A

  // pre-condition
  if(vtktype==VTK_VOID && numComps != 1)
    {
    vtkErrorMacro("Depth component texture must have 1 component only (" <<
                  numComps << " requested");
    return 0;
    }
  const bool oldGeForce=!this->SupportsTextureInteger;

  if(oldGeForce && numComps<3)
    {
    numComps+=2;
    }
  // DON'T DEAL WITH VTK_CHAR as this is platform dependent.
  switch (vtktype)
    {
    case VTK_VOID:
      // numComps can be 3 on GeForce<8.
      return GL_DEPTH_COMPONENT;

    case VTK_SIGNED_CHAR:
      if(this->SupportsTextureInteger && shaderSupportsTextureInt)
        {
        switch (numComps)
          {
          case 1:
            return GL_LUMINANCE8I_EXT;
          case 2:
            return GL_LUMINANCE_ALPHA8I_EXT;
          case 3:
            return GL_RGB8I_EXT;
          case 4:
            return GL_RGBA8I_EXT;
          }
        }
      else
        {
        switch (numComps)
          {
          case 1:
            return GL_LUMINANCE8;
          case 2:
            return GL_LUMINANCE8_ALPHA8;
          case 3:
            return GL_RGB8;
          case 4:
            return GL_RGBA8;
          }
        }

    case VTK_UNSIGNED_CHAR:
      if(this->SupportsTextureInteger && shaderSupportsTextureInt)
        {
        switch (numComps)
          {
          case 1:
            return GL_LUMINANCE8UI_EXT;
          case 2:
            return GL_LUMINANCE_ALPHA8UI_EXT;
          case 3:
            return GL_RGB8UI_EXT;
          case 4:
            return GL_RGBA8UI_EXT;
          }
        }
      else
        {
        switch (numComps)
          {
          case 1:
            return GL_LUMINANCE8;
          case 2:
            return GL_LUMINANCE8_ALPHA8;
          case 3:
            return GL_RGB8;
          case 4:
            return GL_RGBA8;
          }
        }

    case VTK_SHORT:
      if(this->SupportsTextureInteger && shaderSupportsTextureInt)
        {
        switch (numComps)
          {
          case 1:
            return GL_LUMINANCE16I_EXT;
          case 2:
            return GL_LUMINANCE_ALPHA16I_EXT;
          case 3:
            return GL_RGB16I_EXT;
          case 4:
            return GL_RGBA16I_EXT;
          }
        }
      else
        {
        switch (numComps)
          {
          case 1:
            if(this->SupportsTextureFloat)
              {
              return GL_LUMINANCE32F_ARB;
  //            return GL_LUMINANCE16; // not supported as a render target
              }
            else
              {
              vtkGenericWarningMacro("Unsupported type!");
              return 0;
              }
          case 2:
            if(this->SupportsTextureFloat)
              {
              return GL_LUMINANCE_ALPHA32F_ARB;
              //            return GL_LUMINANCE16_ALPHA16; // not supported as a render target
              }
            else
              {
              vtkGenericWarningMacro("Unsupported type!");
              return 0;
              }
          case 3:
            return GL_RGB16;
          case 4:
            return GL_RGBA16;
          }
        }

    case VTK_UNSIGNED_SHORT:
      if(this->SupportsTextureInteger && shaderSupportsTextureInt)
        {
        switch (numComps)
          {
          case 1:
            return GL_LUMINANCE16UI_EXT;
          case 2:
            return GL_LUMINANCE_ALPHA16UI_EXT;
          case 3:
            return GL_RGB16UI_EXT;
          case 4:
            return GL_RGBA16UI_EXT;
          }
        }
      else
        {
        switch (numComps)
          {
          case 1:
            if(this->SupportsTextureFloat)
              {
              return GL_LUMINANCE32F_ARB;
  //      return GL_LUMINANCE16; // not supported as a render target
              }
            else
              {
              vtkGenericWarningMacro("Unsupported type!");
              return 0;
              }
          case 2:
            if(this->SupportsTextureFloat)
              {
              return GL_LUMINANCE_ALPHA32F_ARB;
  //      return GL_LUMINANCE16_ALPHA16; // not supported as a render target
              }
            else
              {
               vtkGenericWarningMacro("Unsupported type!");
               return 0;
              }
          case 3:
            return GL_RGB16;
          case 4:
            return GL_RGBA16;
          }
        }

    case VTK_INT:
      if(this->SupportsTextureInteger && shaderSupportsTextureInt)
        {
        switch (numComps)
          {
          case 1:
            return GL_LUMINANCE32I_EXT;

          case 2:
            return GL_LUMINANCE_ALPHA32I_EXT;

          case 3:
            return GL_RGB32I_EXT;

          case 4:
            return GL_RGBA32I_EXT;
          }
        }
      else
        {
        if(this->SupportsTextureFloat)
          {
          switch (numComps)
            {
            case 1:
              return GL_LUMINANCE32F_ARB;

            case 2:
              return GL_LUMINANCE_ALPHA32F_ARB;

            case 3:
              return GL_RGB32F_ARB;

            case 4:
              return GL_RGBA32F_ARB;
            }
          }
        else
          {
          vtkGenericWarningMacro("Unsupported type!");
          return 0;
          }
        }

    case VTK_UNSIGNED_INT:
      if(this->SupportsTextureInteger && shaderSupportsTextureInt)
        {
        switch (numComps)
          {
          case 1:
            return GL_LUMINANCE32UI_EXT;

          case 2:
            return GL_LUMINANCE_ALPHA32UI_EXT;

          case 3:
            return GL_RGB32UI_EXT;

          case 4:
            return GL_RGBA32UI_EXT;
          }
        }
      else
        {
        if(this->SupportsTextureFloat)
          {
          switch (numComps)
            {
            case 1:
              return GL_LUMINANCE32F_ARB;

            case 2:
              return GL_LUMINANCE_ALPHA32F_ARB;

            case 3:
              return GL_RGB32F_ARB;

            case 4:
              return GL_RGBA32F_ARB;
            }
          }
        else
          {
          vtkGenericWarningMacro("Unsupported type!");
          return 0;
          }
        }

    case VTK_FLOAT:
      if(this->SupportsTextureFloat)
        {
        switch (numComps)
          {
          case 1:
            return GL_LUMINANCE32F_ARB;

          case 2:
            return GL_LUMINANCE_ALPHA32F_ARB;

          case 3:
            return GL_RGB32F_ARB;

          case 4:
            return GL_RGBA32F_ARB;
          }
        }
      else
        {
        vtkGenericWarningMacro("Unsupported type!");
        return 0;
        }
    case VTK_DOUBLE:
      vtkGenericWarningMacro("Unsupported type double!");
    }
  return 0;
}

#else

//----------------------------------------------------------------------------
unsigned int vtkTextureObject::GetInternalFormat(int vtktype, int numComps,
                                                 bool shaderSupportsTextureInt)
{
  // pre-condition
  if(vtktype==VTK_VOID && numComps != 1)
    {
    vtkErrorMacro("Depth component texture must have 1 component only (" <<
                  numComps << " requested");
    return 0;
    }

  // DON'T DEAL WITH VTK_CHAR as this is platform dependent.
  switch (vtktype)
    {
    case VTK_VOID:
      // numComps can be 3 on GeForce<8.
      return GL_DEPTH_COMPONENT;

    case VTK_UNSIGNED_CHAR:
    case VTK_SIGNED_CHAR:
      switch (numComps)
        {
        case 1:
          return GL_LUMINANCE;
        case 2:
          return GL_LUMINANCE_ALPHA;
        case 3:
          return GL_RGB;
        case 4:
          return GL_RGBA;
        }

    case VTK_SHORT:
    case VTK_UNSIGNED_SHORT:
    case VTK_INT:
    case VTK_UNSIGNED_INT:
    case VTK_DOUBLE:
      vtkGenericWarningMacro("Unsupported texture type!");
    }
  return 0;
}

#endif

unsigned int vtkTextureObject::GetFormat(int vtktype, int numComps,
                                         bool shaderSupportsTextureInt)
{
  if (vtktype == VTK_VOID)
    {
    return GL_DEPTH_COMPONENT;
    }

#if GL_ES_VERSION_2_0 != 1
  if(this->SupportsTextureInteger && shaderSupportsTextureInt
     && (vtktype==VTK_SIGNED_CHAR||vtktype==VTK_UNSIGNED_CHAR||
         vtktype==VTK_SHORT||vtktype==VTK_UNSIGNED_SHORT||vtktype==VTK_INT||
       vtktype==VTK_UNSIGNED_INT))
    {
    switch (numComps)
      {
      case 1:
        return GL_LUMINANCE_INTEGER_EXT;
      case 2:
        return GL_LUMINANCE_ALPHA_INTEGER_EXT;
      case 3:
        return GL_RGB_INTEGER_EXT;
      case 4:
        return GL_RGBA_INTEGER_EXT;
      }
    }
  else
#endif
    {
    switch (numComps)
      {
      case 1:
        return GL_LUMINANCE;
      case 2:
        return GL_LUMINANCE_ALPHA;
      case 3:
        return GL_RGB;
      case 4:
        return GL_RGBA;
      }
    }
  return 0;
}

static GLenum vtkGetType(int vtk_scalar_type)
{
  // DON'T DEAL with VTK_CHAR as this is platform dependent.

  switch (vtk_scalar_type)
    {
  case VTK_SIGNED_CHAR:
    return GL_BYTE;

  case VTK_UNSIGNED_CHAR:
    return GL_UNSIGNED_BYTE;

  case VTK_SHORT:
    return GL_SHORT;

  case VTK_UNSIGNED_SHORT:
    return GL_UNSIGNED_SHORT;

  case VTK_INT:
    return GL_INT;

  case VTK_UNSIGNED_INT:
    return GL_UNSIGNED_INT;

  case VTK_FLOAT:
  case VTK_VOID: // used for depth component textures.
    return GL_FLOAT;
    }
  return 0;
}

static int vtkGetVTKType(GLenum gltype)
{
   // DON'T DEAL with VTK_CHAR as this is platform dependent.
  switch (gltype)
    {
  case GL_BYTE:
    return VTK_SIGNED_CHAR;

  case GL_UNSIGNED_BYTE:
    return VTK_UNSIGNED_CHAR;

  case GL_SHORT:
    return VTK_SHORT;

  case GL_UNSIGNED_SHORT:
    return VTK_UNSIGNED_SHORT;

  case GL_INT:
    return VTK_INT;

  case GL_UNSIGNED_INT:
    return VTK_UNSIGNED_INT;

  case GL_FLOAT:
    return VTK_FLOAT;
    }

  return 0;
}

//----------------------------------------------------------------------------
int vtkTextureObject::GetDataType()
{
  return ::vtkGetVTKType(this->Type);
}


#if GL_ES_VERSION_2_0 != 1

//----------------------------------------------------------------------------
bool vtkTextureObject::Create1D(int numComps,
                                vtkPixelBufferObject* pbo,
                                bool shaderSupportsTextureInt)
{
  assert(this->Context);
  assert(pbo->GetContext() == this->Context.GetPointer());

  GLenum target = GL_TEXTURE_1D;

  // Now, detemine texture parameters using the information from the pbo.

  // * internalFormat depends on number of components and the data type.
  GLenum internalFormat = this->GetInternalFormat(pbo->GetType(), numComps,
                                                 shaderSupportsTextureInt);

  // * format depends on the number of components.
  GLenum format = this->GetFormat(pbo->GetType(), numComps,
                                 shaderSupportsTextureInt);

  // * type if the data type in the pbo
  GLenum type = ::vtkGetType(pbo->GetType());

  if (!internalFormat || !format || !type)
    {
    vtkErrorMacro("Failed to detemine texture parameters.");
    return false;
    }

  this->Target = target;
  this->CreateTexture();
  this->Bind();

  pbo->Bind(vtkPixelBufferObject::UNPACKED_BUFFER);

  // Source texture data from the PBO.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage1D(target, 0, static_cast<GLint>(internalFormat),
               static_cast<GLsizei>(pbo->GetSize()/
                                    static_cast<unsigned int>(numComps)),
               0, format,
               type, BUFFER_OFFSET(0));
  vtkOpenGLCheckErrorMacro("failed at glTexImage1D");
  pbo->UnBind();
  this->UnBind();

  this->Target = target;
  this->Format = format;
  this->Type = type;
  this->Components = numComps;
  this->Width = pbo->GetSize();
  this->Height = 1;
  this->Depth =1;
  this->NumberOfDimensions=1;
  return true;
}

//----------------------------------------------------------------------------
bool vtkTextureObject::Create2D(unsigned int width, unsigned int height,
                                int numComps, vtkPixelBufferObject* pbo,
                                bool shaderSupportsTextureInt)
{
  assert(this->Context);
  assert(pbo->GetContext() == this->Context.GetPointer());

  if (pbo->GetSize() < width*height*static_cast<unsigned int>(numComps))
    {
    vtkErrorMacro("PBO size must match texture size.");
    return false;
    }

  // Now, detemine texture parameters using the information from the pbo.
  // * internalFormat depends on number of components and the data type.
  // * format depends on the number of components.
  // * type if the data type in the pbo

  int vtktype = pbo->GetType();
  GLenum type = ::vtkGetType(vtktype);

  GLenum internalFormat
    = this->GetInternalFormat(vtktype, numComps, shaderSupportsTextureInt);

  GLenum format
    = this->GetFormat(vtktype, numComps, shaderSupportsTextureInt);

  if (!internalFormat || !format || !type)
    {
    vtkErrorMacro("Failed to detemine texture parameters.");
    return false;
    }

  GLenum target = GL_TEXTURE_2D;
  this->Target = target;
  this->CreateTexture();
  this->Bind();

  // Source texture data from the PBO.
  pbo->Bind(vtkPixelBufferObject::UNPACKED_BUFFER);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glTexImage2D(
        target,
        0,
        internalFormat,
        static_cast<GLsizei>(width),
        static_cast<GLsizei>(height),
        0,
        format,
        type,
        BUFFER_OFFSET(0));

  vtkOpenGLCheckErrorMacro("failed at glTexImage2D");

  pbo->UnBind();
  this->UnBind();

  this->Target = target;
  this->Format = format;
  this->Type = type;
  this->Components = numComps;
  this->Width = width;
  this->Height = height;
  this->Depth = 1;
  this->NumberOfDimensions = 2;

  return true;
}

// ----------------------------------------------------------------------------
// Description:
// Create a 2D depth texture using a PBO.
bool vtkTextureObject::CreateDepth(unsigned int width,
                                   unsigned int height,
                                   int internalFormat,
                                   vtkPixelBufferObject *pbo)
{
  assert("pre: context_exists" && this->GetContext()!=0);
  assert("pre: pbo_context_exists" && pbo->GetContext()!=0);
  assert("pre: context_match" && this->GetContext()==pbo->GetContext());
  assert("pre: sizes_match" && pbo->GetSize()==width*height);
  assert("pre: valid_internalFormat" && internalFormat>=0
         && internalFormat<NumberOfDepthFormats);

  GLenum inFormat=OpenGLDepthInternalFormat[internalFormat];
  GLenum type=::vtkGetType(pbo->GetType());

  this->Target=GL_TEXTURE_2D;
  this->Format=GL_DEPTH_COMPONENT;
  this->Type=type;
  this->Width=width;
  this->Height=height;
  this->Depth=1;
  this->NumberOfDimensions=2;
  this->Components=1;

  this->CreateTexture();
  this->Bind();

  pbo->Bind(vtkPixelBufferObject::UNPACKED_BUFFER);

  // Source texture data from the PBO.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(this->Target, 0, static_cast<GLint>(inFormat),
               static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0,
               this->Format, this->Type, BUFFER_OFFSET(0));
  vtkOpenGLCheckErrorMacro("failed at glTexImage2D");
  pbo->UnBind();
  this->UnBind();
  return true;
}

//----------------------------------------------------------------------------
bool vtkTextureObject::Create3D(unsigned int width, unsigned int height,
                                unsigned int depth, int numComps,
                                vtkPixelBufferObject* pbo,
                                bool shaderSupportsTextureInt)
{
#ifdef GL_TEXTURE_3D
  assert(this->Context);
  assert(this->Context.GetPointer() == pbo->GetContext());

  if (pbo->GetSize() != width*height*depth*static_cast<unsigned int>(numComps))
    {
    vtkErrorMacro("PBO size must match texture size.");
    return false;
    }

  GLenum target = GL_TEXTURE_3D;

  // Now, detemine texture parameters using the information from the pbo.

  // * internalFormat depends on number of components and the data type.
  GLenum internalFormat = this->GetInternalFormat(pbo->GetType(), numComps,
                                                 shaderSupportsTextureInt);

  // * format depends on the number of components.
  GLenum format = this->GetFormat(pbo->GetType(), numComps,
                                  shaderSupportsTextureInt);

  // * type if the data type in the pbo
  GLenum type = ::vtkGetType(pbo->GetType());

  if (!internalFormat || !format || !type)
    {
    vtkErrorMacro("Failed to detemine texture parameters.");
    return false;
    }

  this->Target = target;
  this->CreateTexture();
  this->Bind();

  pbo->Bind(vtkPixelBufferObject::UNPACKED_BUFFER);

  // Source texture data from the PBO.
  glTexImage3D(target, 0, static_cast<GLint>(internalFormat),
                    static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                    static_cast<GLsizei>(depth), 0, format, type,
                    BUFFER_OFFSET(0));

  vtkOpenGLCheckErrorMacro("failed at glTexImage3D");

  pbo->UnBind();
  this->UnBind();

  this->Target = target;
  this->Format = format;
  this->Type = type;
  this->Components = numComps;
  this->Width = width;
  this->Height = height;
  this->Depth = depth;
  this->NumberOfDimensions = 3;
  return true;

  #else
    return false;
  #endif
}

//----------------------------------------------------------------------------
vtkPixelBufferObject* vtkTextureObject::Download()
{
  assert(this->Context);
  assert(this->Handle);

  vtkPixelBufferObject* pbo = vtkPixelBufferObject::New();
  pbo->SetContext(this->Context);

  int vtktype = ::vtkGetVTKType(this->Type);
  if (vtktype == 0)
    {
    vtkErrorMacro("Failed to determine type.");
    return 0;
    }

  unsigned int size = this->Width* this->Height* this->Depth;

  // doesn't matter which Upload*D method we use since we are not really
  // uploading any data, simply allocating GPU space.
  if (!pbo->Upload1D(vtktype, NULL, size, this->Components, 0))
    {
    vtkErrorMacro("Could not allocate memory for PBO.");
    pbo->Delete();
    return 0;
    }

  pbo->Bind(vtkPixelBufferObject::PACKED_BUFFER);
  this->Bind();

#if GL_ES_VERSION_2_0 != 1
  glGetTexImage(this->Target, 0, this->Format, this->Type, BUFFER_OFFSET(0));
#else
  // you can do something with glReadPixels and binding a texture as a FBO
  // I believe for ES 2.0
#endif

  vtkOpenGLCheckErrorMacro("failed at glGetTexImage");
  this->UnBind();
  pbo->UnBind();

  pbo->SetComponents(this->Components);

  return pbo;
}

#endif

//----------------------------------------------------------------------------
bool vtkTextureObject::Create2DFromRaw(unsigned int width, unsigned int height,
                                       int numComps, int dataType, void *data)
{
  assert(this->Context);

  // Now, detemine texture parameters using the arguments.
  GLenum type = ::vtkGetType(dataType);
  GLenum internalFormat
    = this->GetInternalFormat(dataType, numComps, false);
  GLenum format
    = this->GetFormat(dataType, numComps, false);

  if (!internalFormat || !format || !type)
    {
    vtkErrorMacro("Failed to detemine texture parameters.");
    return false;
    }

  GLenum target = GL_TEXTURE_2D;
  this->Target = target;
  this->CreateTexture();
  this->Bind();

  // Source texture data from the PBO.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  glTexImage2D(
        target,
        0,
        internalFormat,
        static_cast<GLsizei>(width),
        static_cast<GLsizei>(height),
        0,
        format,
        type,
        static_cast<const GLvoid *>(data));

  vtkOpenGLCheckErrorMacro("failed at glTexImage2D");

  this->UnBind();

  this->Target = target;
  this->Format = format;
  this->Type = type;
  this->Components = numComps;
  this->Width = width;
  this->Height = height;
  this->Depth = 1;
  this->NumberOfDimensions = 2;

  return true;
}


// ----------------------------------------------------------------------------
// Description:
// Create a 2D depth texture using a raw pointer.
// This is a blocking call. If you can, use PBO instead.
bool vtkTextureObject::CreateDepthFromRaw(unsigned int width,
                                          unsigned int height,
                                          int internalFormat,
                                          int rawType,
                                          void *raw)
{
  assert("pre: context_exists" && this->GetContext()!=0);
  assert("pre: raw_exists" && raw!=0);

  assert("pre: valid_internalFormat" && internalFormat>=0
         && internalFormat<NumberOfDepthFormats);

  GLenum inFormat=OpenGLDepthInternalFormat[internalFormat];
  GLenum type=::vtkGetType(rawType);

  this->Target=GL_TEXTURE_2D;
  this->Format=GL_DEPTH_COMPONENT;
  this->Type=type;
  this->Width=width;
  this->Height=height;
  this->Depth=1;
  this->NumberOfDimensions=2;
  this->Components=1;

  this->CreateTexture();
  this->Bind();

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(this->Target, 0, static_cast<GLint>(inFormat),
               static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0,
               this->Format, this->Type,raw);
  vtkOpenGLCheckErrorMacro("failed at glTexImage2D");
  this->UnBind();
  return true;
}

// ----------------------------------------------------------------------------
bool vtkTextureObject::AllocateDepth(unsigned int width,unsigned int height,
                                     int internalFormat)
{
  assert("pre: context_exists" && this->GetContext()!=0);
  assert("pre: valid_internalFormat" && internalFormat>=0
         && internalFormat<NumberOfDepthFormats);

  this->Target=GL_TEXTURE_2D;
  this->Format=GL_DEPTH_COMPONENT;
  // try to match vtk type to internal fmt
  this->Type=OpenGLDepthInternalFormatType[internalFormat];
  this->Width=width;
  this->Height=height;
  this->Depth=1;
  this->NumberOfDimensions=2;
  this->Components=1;

  this->CreateTexture();
  this->Bind();

  GLenum inFormat=OpenGLDepthInternalFormat[internalFormat];
  glTexImage2D(
          this->Target,
          0,
          static_cast<GLint>(inFormat),
          static_cast<GLsizei>(width),
          static_cast<GLsizei>(height),
          0,
          this->Format,
          this->Type,
          0);

  vtkOpenGLCheckErrorMacro("failed at glTexImage2D");

  this->UnBind();
  return true;
}

// ----------------------------------------------------------------------------
bool vtkTextureObject::Allocate1D(unsigned int width, int numComps,
                                  int vtkType)
{
#ifdef GL_TEXTURE_1D
  assert(this->Context);

  this->Target=GL_TEXTURE_1D;
  GLenum internalFormat = this->GetInternalFormat(vtkType, numComps,
                                                  false);

  // don't care, allocation only, no data transfer
  GLenum format = this->GetFormat(vtkType, numComps,false);

  GLenum type = ::vtkGetType(vtkType);

  this->Format = format;
  this->Type = type;
  this->Components = numComps;
  this->Width = width;
  this->Height = 1;
  this->Depth =1;
  this->NumberOfDimensions=1;

  this->CreateTexture();
  this->Bind();
  glTexImage1D(this->Target, 0, static_cast<GLint>(internalFormat),
               static_cast<GLsizei>(width),0, format, type,0);
  vtkOpenGLCheckErrorMacro("failed at glTexImage1D");
  this->UnBind();
  return true;
#else
  return false;
#endif
}

// ----------------------------------------------------------------------------
// Description:
// Create a 2D color texture but does not initialize its values.
// Internal format is deduced from numComps and vtkType.
bool vtkTextureObject::Allocate2D(unsigned int width,unsigned int height,
                                  int numComps,int vtkType)
{
  assert(this->Context);

  this->Target=GL_TEXTURE_2D;

  GLenum internalFormat = this->GetInternalFormat(vtkType, numComps,
                                                  false);

  // don't care, allocation only, no data transfer
  GLenum format = this->GetFormat(vtkType, numComps,false);

  GLenum type = ::vtkGetType(vtkType);

  this->Format = format;
  this->Type = type;
  this->Components = numComps;
  this->Width = width;
  this->Height = height;
  this->Depth =1;
  this->NumberOfDimensions=2;

  this->CreateTexture();
  this->Bind();
  glTexImage2D(this->Target, 0, static_cast<GLint>(internalFormat),
               static_cast<GLsizei>(width), static_cast<GLsizei>(height),
               0, format, type,0);
  vtkOpenGLCheckErrorMacro("failed at glTexImage2D");
  this->UnBind();
  return true;
}

// ----------------------------------------------------------------------------
// Description:
// Create a 3D color texture but does not initialize its values.
// Internal format is deduced from numComps and vtkType.
bool vtkTextureObject::Allocate3D(unsigned int width,unsigned int height,
                                  unsigned int depth, int numComps,
                                  int vtkType)
{
#ifdef GL_TEXTURE_3D
  this->Target=GL_TEXTURE_3D;

  if(this->Context==0)
    {
    vtkErrorMacro("No context specified. Cannot create texture.");
    return false;
    }
  GLenum internalFormat = this->GetInternalFormat(vtkType, numComps,
                                                  false);

  // don't care, allocation only, no data transfer
  GLenum format = this->GetFormat(vtkType, numComps,false);

  GLenum type = ::vtkGetType(vtkType);

  this->Format = format;
  this->Type = type;
  this->Components = numComps;
  this->Width = width;
  this->Height = height;
  this->Depth =depth;
  this->NumberOfDimensions=3;

  this->CreateTexture();
  this->Bind();
  glTexImage3D(this->Target, 0, static_cast<GLint>(internalFormat),
                    static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                    static_cast<GLsizei>(depth), 0, format, type,0);
  vtkOpenGLCheckErrorMacro("failed at glTexImage3D");
  this->UnBind();
  return true;
#else
  return false;
#endif
}


//----------------------------------------------------------------------------
bool vtkTextureObject::Create2D(unsigned int width, unsigned int height,
                                int numComps, int vtktype,
                                bool shaderSupportsTextureInt)
{
  assert(this->Context);

  GLenum target = GL_TEXTURE_2D;

  // Now, detemine texture parameters using the information provided.
  // * internalFormat depends on number of components and the data type.
  GLenum internalFormat = this->GetInternalFormat(vtktype, numComps,
                                                 shaderSupportsTextureInt);

  // * format depends on the number of components.
  GLenum format = this->GetFormat(vtktype, numComps,
                                 shaderSupportsTextureInt);

  // * type if the data type in the pbo
  GLenum type = ::vtkGetType(vtktype);

  if (!internalFormat || !format || !type)
    {
    vtkErrorMacro("Failed to detemine texture parameters.");
    return false;
    }

  this->Target = target;
  this->CreateTexture();
  this->Bind();

  // Allocate space for texture, don't upload any data.
  glTexImage2D(target, 0, static_cast<GLint>(internalFormat),
               static_cast<GLsizei>(width), static_cast<GLsizei>(height),
               0, format, type, NULL);
  vtkOpenGLCheckErrorMacro("failed at glTexImage2D");
  this->UnBind();

  this->Target = target;
  this->Format = format;
  this->Type = type;
  this->Components = numComps;
  this->Width = width;
  this->Height = height;
  this->Depth = 1;
  this->NumberOfDimensions = 2;
  return true;
}

//----------------------------------------------------------------------------
bool vtkTextureObject::Create3D(unsigned int width, unsigned int height,
                                unsigned int depth,
                                int numComps, int vtktype,
                                bool shaderSupportsTextureInt)
{
#ifdef GL_TEXTURE_3D
  assert(this->Context);

  GLenum target = GL_TEXTURE_3D;

  // Now, detemine texture parameters using the information provided.
  // * internalFormat depends on number of components and the data type.
  GLenum internalFormat = this->GetInternalFormat(vtktype, numComps,
                                                 shaderSupportsTextureInt);

  // * format depends on the number of components.
  GLenum format = this->GetFormat(vtktype, numComps,
                                 shaderSupportsTextureInt);

  // * type if the data type in the pbo
  GLenum type = ::vtkGetType(vtktype);

  if (!internalFormat || !format || !type)
    {
    vtkErrorMacro("Failed to detemine texture parameters.");
    return false;
    }

  this->Target = target;
  this->CreateTexture();
  this->Bind();

  // Allocate space for texture, don't upload any data.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage3D(target, 0, static_cast<GLint>(internalFormat),
                    static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                    static_cast<GLsizei>(depth), 0, format, type, NULL);
  vtkOpenGLCheckErrorMacro("falied at glTexImage3D");
  this->UnBind();

  this->Target = target;
  this->Format = format;
  this->Type = type;
  this->Components = numComps;
  this->Width = width;
  this->Height = height;
  this->Depth = depth;
  this->NumberOfDimensions = 3;
  return true;
#else
  return false;
#endif
}

// ----------------------------------------------------------------------------
void vtkTextureObject::CopyToFrameBuffer(int srcXmin,
                                         int srcYmin,
                                         int srcXmax,
                                         int srcYmax,
                                         int dstXmin,
                                         int dstYmin,
                                         int width,
                                         int height)
{
  assert("pre: positive_srcXmin" && srcXmin>=0);
  assert("pre: max_srcXmax" &&
         static_cast<unsigned int>(srcXmax)<this->GetWidth());
  assert("pre: increasing_x" && srcXmin<=srcXmax);
  assert("pre: positive_srcYmin" && srcYmin>=0);
  assert("pre: max_srcYmax" &&
         static_cast<unsigned int>(srcYmax)<this->GetHeight());
  assert("pre: increasing_y" && srcYmin<=srcYmax);
  assert("pre: positive_dstXmin" && dstXmin>=0);
  assert("pre: positive_dstYmin" && dstYmin>=0);
  assert("pre: positive_width" && width>0);
  assert("pre: positive_height" && height>0);
  assert("pre: x_fit" && dstXmin+(srcXmax-srcXmin)<width);
  assert("pre: y_fit" && dstYmin+(srcYmax-srcYmin)<height);

  vtkOpenGLClearErrorMacro();

  if (this->DrawPixelsActor == NULL)
    {
    this->DrawPixelsActor = vtkTexturedActor2D::New();
    vtkNew<vtkPolyDataMapper2D> mapper;
    vtkNew<vtkPolyData> polydata;
    vtkNew<vtkPoints> points;
    points->SetNumberOfPoints(4);
    polydata->SetPoints(points.Get());

    vtkNew<vtkCellArray> tris;
    tris->InsertNextCell(3);
    tris->InsertCellPoint(0);
    tris->InsertCellPoint(1);
    tris->InsertCellPoint(2);
    tris->InsertNextCell(3);
    tris->InsertCellPoint(0);
    tris->InsertCellPoint(2);
    tris->InsertCellPoint(3);
    polydata->SetPolys(tris.Get());

    vtkNew<vtkTrivialProducer> prod;
    prod->SetOutput(polydata.Get());

    // Set some properties.
    mapper->SetInputConnection(prod->GetOutputPort());
    this->DrawPixelsActor->SetMapper(mapper.Get());

    vtkNew<vtkTexture> texture;
    texture->RepeatOff();
    this->DrawPixelsActor->SetTexture(texture.Get());

    vtkNew<vtkFloatArray> tcoords;
    tcoords->SetNumberOfComponents(2);
    tcoords->SetNumberOfTuples(4);
    polydata->GetPointData()->SetTCoords(tcoords.Get());
    }

  GLfloat minXTexCoord=static_cast<GLfloat>(
    static_cast<double>(srcXmin)/this->Width);
  GLfloat minYTexCoord=static_cast<GLfloat>(
    static_cast<double>(srcYmin)/this->Height);

  GLfloat maxXTexCoord=static_cast<GLfloat>(
    static_cast<double>(srcXmax+1)/this->Width);
  GLfloat maxYTexCoord=static_cast<GLfloat>(
    static_cast<double>(srcYmax+1)/this->Height);

  GLfloat dstXmax=static_cast<GLfloat>(dstXmin+srcXmax-srcXmin);
  GLfloat dstYmax=static_cast<GLfloat>(dstYmin+srcYmax-srcYmin);

  vtkPolyData *pd = vtkPolyDataMapper2D::SafeDownCast(this->DrawPixelsActor->GetMapper())->GetInput();
  vtkPoints *points = pd->GetPoints();
  points->SetPoint(0, dstXmin, dstYmin, 0);
  points->SetPoint(1, dstXmax, dstYmin, 0);
  points->SetPoint(2, dstXmax, dstYmax, 0);
  points->SetPoint(3, dstXmin, dstYmax, 0);

  vtkDataArray *tcoords = pd->GetPointData()->GetTCoords();
  float tmp[2];
  tmp[0] = minXTexCoord;
  tmp[1] = minYTexCoord;
  tcoords->SetTuple(0,tmp);
  tmp[0] = maxXTexCoord;
  tcoords->SetTuple(1,tmp);
  tmp[1] = maxYTexCoord;
  tcoords->SetTuple(2,tmp);
  tmp[0] = minXTexCoord;
  tcoords->SetTuple(3,tmp);

  vtkOpenGLTexture::SafeDownCast(this->DrawPixelsActor->GetTexture())->SetTextureObject(this);

  glDisable( GL_SCISSOR_TEST );

  vtkRenderer *vp = vtkRenderer::New();
  this->Context->AddRenderer(vp);
  this->DrawPixelsActor->RenderOverlay(vp);
  this->Context->RemoveRenderer(vp);
  vp->Delete();

  vtkOpenGLCheckErrorMacro("failed after CopyToFrameBuffer")
}

//----------------------------------------------------------------------------
// Description:
// Copy a sub-part of a logical buffer of the framebuffer (color or depth)
// to the texture object. src is the framebuffer, dst is the texture.
// (srcXmin,srcYmin) is the location of the lower left corner of the
// rectangle in the framebuffer. (dstXmin,dstYmin) is the location of the
// lower left corner of the rectangle in the texture. width and height
// specifies the size of the rectangle in pixels.
// If the logical buffer is a color buffer, it has to be selected first with
// glReadBuffer().
// \pre is2D: GetNumberOfDimensions()==2
void vtkTextureObject::CopyFromFrameBuffer(int srcXmin,
                                           int srcYmin,
                                           int vtkNotUsed(dstXmin),
                                           int vtkNotUsed(dstYmin),
                                           int width,
                                           int height)
{
  assert("pre: is2D" && this->GetNumberOfDimensions()==2);

#if 1
  this->Activate();
  glCopyTexImage2D(this->Target,0,this->Format,srcXmin,srcYmin,width,height,0);
  vtkOpenGLCheckErrorMacro("failed at glCopyTexImage2D");

#else

  this->Bind();
  glCopyTexSubImage2D(this->Target,0,dstXmin,dstYmin,srcXmin,srcYmin,width,
                      height);
  vtkOpenGLCheckErrorMacro("failed at glCopyTexSubImage2D");
  this->UnBind();
#endif

}

//----------------------------------------------------------------------------
void vtkTextureObject::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Width: " << this->Width << endl;
  os << indent << "Height: " << this->Height << endl;
  os << indent << "Depth: " << this->Depth << endl;
  os << indent << "Components: " << this->Components << endl;
  os << indent << "Handle: " << this->Handle << endl;
  os << indent << "Target: ";

  switch(this->Target)
    {
#ifdef GL_TEXTURE_1D
    case GL_TEXTURE_1D:
      os << "GL_TEXTURE_1D" << endl;
      break;
#endif
    case  GL_TEXTURE_2D:
      os << "GL_TEXTURE_2D" << endl;
      break;
#ifdef GL_TEXTURE_3D
    case  GL_TEXTURE_3D:
      os << "GL_TEXTURE_3D" << endl;
      break;
#endif
    default:
      os << "unknown value: 0x" << hex << this->Target << dec <<endl;
      break;
    }

  os << indent << "NumberOfDimensions: " << this->NumberOfDimensions << endl;

  os << indent << "WrapS: " << WrapAsString[this->WrapS] << endl;
  os << indent << "WrapT: " << WrapAsString[this->WrapT] << endl;
  os << indent << "WrapR: " << WrapAsString[this->WrapR] << endl;

  os << indent << "MinificationFilter: "
     << MinMagFilterAsString[this->MinificationFilter] << endl;

  os << indent << "MagnificationFilter: "
     << MinMagFilterAsString[this->MagnificationFilter] << endl;

  os << indent << "LinearMagnification: " << this->LinearMagnification << endl;

  os << indent << "Priority: " << this->Priority <<  endl;
  os << indent << "MinLOD: " << this->MinLOD << endl;
  os << indent << "MaxLOD: " << this->MaxLOD << endl;
  os << indent << "BaseLevel: " << this->BaseLevel << endl;
  os << indent << "MaxLevel: " << this->MaxLevel << endl;
  os << indent << "DepthTextureCompare: " << this->DepthTextureCompare
     << endl;
  os << indent << "DepthTextureCompareFunction: "
     << DepthTextureCompareFunctionAsString[this->DepthTextureCompareFunction]
     << endl;
  os << indent << "DepthTextureMode: "
     << DepthTextureModeAsString[this->DepthTextureMode] << endl;
  os << indent << "GenerateMipmap: " << this->GenerateMipmap << endl;
}
