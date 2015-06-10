
comments = """

Based on the C++ example here and from the vtk mailing lists:

http://stackoverflow.com/questions/24615455/add-glsl-shader-to-a-vtkactor-vtk-6-1

in Slicer superbuild tree, paste in this to test:

execfile('./VTKv6/Rendering/OpenGL/Testing/Python/NoiseShaders.py')

See this post about changes in the API - none of the existing
tests seem to cover this topic from python:

http://vtk.1045678.n5.nabble.com/vtkProperty-LoadMaterial-disappeared-in-VTK-6-1-td5726066.html

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


fragmentShaderNoiseSource = """
// http://glslsandbox.com/e#25816.0

uniform float time;
uniform vec2 resolution;

float snoise(vec3 uv, float res)
{
  const vec3 s = vec3(1e0, 1e2, 1e3);

  uv *= res;

  vec3 uv0 = floor(mod(uv, res))*s;
  vec3 uv1 = floor(mod(uv+vec3(1.), res))*s;

  vec3 f = fract(uv); f = f*f*(3.0-2.0*f);

  vec4 v = vec4(uv0.x+uv0.y+uv0.z, uv1.x+uv0.y+uv0.z,
                  uv0.x+uv1.y+uv0.z, uv1.x+uv1.y+uv0.z);

  vec4 r = fract(sin(v*1e-1)*1e3);
  float r0 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);

  r = fract(sin((v + uv1.z - uv0.z)*1e-1)*1e3);
  float r1 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);

  return mix(r0, r1, f.z)*2.-1.;
}

void propFuncFS( void ) {

  vec2 p = .2 * (-.5 + gl_FragCoord.xy / resolution.xy);
  p.x *= resolution.x/resolution.y;

  float color = 3.0 - (3.*length(2.*p));

  vec3 coord = vec3(atan(p.x,p.y)/6.2832+.5, length(p)*.4, .5);

  for(int i = 1; i <= 1; i++)
  {
    float power = pow(2.0, float(i));
    color += (1.5 / power) * snoise(coord + vec3(0.,-time*.05, time*.01), power*16.);
  }
  gl_FragColor = vec4( pow(max(color,0.),2.)*0.4, pow(max(color,0.),3.)*0.15 ,color, 1.0);
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
fragmentShader.SetSourceCode(fragmentShaderNoiseSource)
fragmentShader.SetContext(shaderProgram.GetContext())

shaderProgram.GetShaders().AddItem(vertexShader)
shaderProgram.GetShaders().AddItem(fragmentShader)

# associate the shader with the cube and set variables
openGLproperty = cubeActor.GetProperty()
openGLproperty.SetTexture(vtk.vtkProperty.VTK_TEXTURE_UNIT_1, circleTexture)
openGLproperty.SetPropProgram(shaderProgram)
resolution = [100.,100.]
openGLproperty.AddShaderVariable("resolution", 2, resolution)
openGLproperty.ShadingOn()

cubeMapper.MapDataArrayToVertexAttribute("brownianVectors", "BrownianVectors", 0, -1)
cubeMapper.MapDataArrayToVertexAttribute("textureCoordinates", "TCoords", 0, -1)

# render

for i in xrange(2):
  openGLproperty.AddShaderVariable("time", 1, [1.0* i,])
  cubeActor.RotateZ(1)
  cubeActor.RotateX(1)
  renderWindow.Render()
