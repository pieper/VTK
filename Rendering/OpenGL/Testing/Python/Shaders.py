
comments = """

Based on the C++ example here and from the vtk mailing lists:

http://stackoverflow.com/questions/24615455/add-glsl-shader-to-a-vtkactor-vtk-6-1

in Slicer superbuild tree, paste in this to test:

execfile('./VTKv6/Rendering/OpenGL/Testing/Python/Shaders.py')

"""

# start with a cube that has texture coordinates named "TCoords"
cube = vtk.vtkCubeSource()

# add a random vector fields called "BrownianVectors"
cubeWithVectors = vtk.vtkBrownianPoints()
cubeWithVectors.SetMinimumSpeed(0)
cubeWithVectors.SetMaximumSpeed(1)
cubeWithVectors.SetInputConnection(cube.GetOutputPort())

# create the render-related classes
cubeMapper = vtk.vtkPolyDataMapper()
cubeMapper.SetInputConnection( cubeWithVectors.GetOutputPort() )

cubeActor = vtk.vtkActor()
cubeActor.SetMapper( cubeMapper )

renderer= vtk.vtkRenderer()
renderer.AddActor( cubeActor )

renderWindow = vtk.vtkRenderWindow()
renderWindow.AddRenderer( renderer )

# make a texture (2D circle)
textureSource = vtk.vtkImageEllipsoidSource()
textureSource.SetOutValue(50)
circleTexture = vtk.vtkTexture()
circleTexture.SetInputConnection(textureSource.GetOutputPort())

# a vertex shader that projects the vertex
# and turns the random vectors into colors
vertexShaderSource = """
  attribute vec3 brownianVectors;
  attribute vec2 textureCoordinates;
  varying vec4 colorFromVertex;
  varying vec2 textureCoordinatesFromVertex;
  void propFuncVS(void)
  {
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    colorFromVertex = vec4(normalize(brownianVectors), 0.3);
    textureCoordinatesFromVertex = textureCoordinates;
  }
"""

# a fragment shader that uses the varying color and the texture
fragmentShaderSource = """
  uniform vec4 rgba;
  uniform int useTexture;
  uniform sampler2D textureUnit;
  varying vec4 colorFromVertex;
  varying vec2 textureCoordinatesFromVertex;

  void propFuncFS()
  {
    if(useTexture==1)
      {
      gl_FragColor = 0.5 * colorFromVertex +
                     0.5 * rgba*texture2D(textureUnit,textureCoordinatesFromVertex);
      }
    else
      {
      gl_FragColor = colorFromVertex + rgba;
      }
  }
"""

# create the shader program
shaderProgram = vtk.vtkShaderProgram2()
shaderProgram.SetContext(renderWindow)

vertexShader=vtk.vtkShader2()
vertexShader.SetType(vtk.VTK_SHADER_TYPE_VERTEX)
vertexShader.SetSourceCode(vertexShaderSource)
vertexShader.SetContext(shaderProgram.GetContext())

fragmentShader=vtk.vtkShader2()
fragmentShader.SetType(vtk.VTK_SHADER_TYPE_FRAGMENT)
fragmentShader.SetSourceCode(fragmentShaderSource)
fragmentShader.SetContext(shaderProgram.GetContext())

shaderProgram.GetShaders().AddItem(vertexShader)
shaderProgram.GetShaders().AddItem(fragmentShader)

# associate the shader with the cube and set variables
openGLproperty = cubeActor.GetProperty()
openGLproperty.SetTexture(vtk.vtkProperty.VTK_TEXTURE_UNIT_1, circleTexture)
openGLproperty.SetPropProgram(shaderProgram)
rgba = [0., .7, .7, 1.]
openGLproperty.AddShaderVariable("rgba", 4, rgba)
openGLproperty.AddShaderVariable("useTexture", 1, [1,])
openGLproperty.AddShaderVariable("textureUnit", 1, [1,])
openGLproperty.ShadingOn()

cubeMapper.MapDataArrayToVertexAttribute("brownianVectors", "BrownianVectors", 0, -1)
cubeMapper.MapDataArrayToVertexAttribute("textureCoordinates", "TCoords", 0, -1)

# render
renderWindow.Render()
