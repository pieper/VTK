
comments = """

Based on the C++ example here and from the vtk mailing lists:

http://stackoverflow.com/questions/24615455/add-glsl-shader-to-a-vtkactor-vtk-6-1

in Slicer superbuild tree, paste in this to test:

execfile('./VTKv6/Rendering/OpenGL2/Testing/Python/Shaders.py')

"""

import vtk
import vtk.test.Testing

class TestShaders(vtk.test.Testing.vtkTest):

    def testShaders(self):

        cube = vtk.vtkCubeSource()

        cubeMapper = vtk.vtkPolyDataMapper()
        cubeMapper.SetInputConnection( cube.GetOutputPort() )

        cubeActor = vtk.vtkActor()
        cubeActor.SetMapper( cubeMapper )

        renderer= vtk.vtkRenderer()
        renderer.AddActor( cubeActor )

        renderWindow = vtk.vtkRenderWindow()
        renderWindow.AddRenderer( renderer )

        textureSource = vtk.vtkImageEllipsoidSource()
        textureSource.SetOutValue(50)
        texture = vtk.vtkTexture()
        texture.SetInputConnection(textureSource.GetOutputPort())


        fragmentShaderSource = """
        uniform vec4 rgba;
        uniform int useTexture;
        uniform sampler2D texture;

        void propFuncFS()
        {
          if(useTexture==1)
            {
            gl_FragColor=rgba*texture2D(texture,gl_TexCoord[0].xy);
            }
          else
            {
            gl_FragColor=rgba;
            }
        }
        """

        shaderProgram = vtk.vtkShaderProgram2()
        shaderProgram.SetContext(renderWindow)

        shader=vtk.vtkShader2()
        shader.SetType(vtk.VTK_SHADER_TYPE_FRAGMENT)
        shader.SetSourceCode(fragmentShaderSource)
        shader.SetContext(shaderProgram.GetContext())

        shaderProgram.GetShaders().AddItem(shader)


        openGLproperty = cubeActor.GetProperty()
        openGLproperty.SetTexture(0, texture)
        openGLproperty.SetPropProgram(shaderProgram)
        rgba = [0., .7, .7, 1.]
        openGLproperty.AddShaderVariable("rgba", 4, rgba)
        openGLproperty.AddShaderVariable("useTexture", 1, [1,])
        openGLproperty.ShadingOn()

        renderWindow.Render()

        baseline = vtk.test.Testing.getAbsImagePath("Shaders.png")
        vtk.test.Testing.compareImage(renderWindow, baseline, threshold=25)

if __name__ == "__main__":
     vtk.test.Testing.main([(TestShaders, 'test')])
