#include <iostream.h>
#include "vtkImageViewer.h"
#include "vtkImageReader.h"
#include "vtkImageImport.h"
#include "vtkImageExport.h"
#include "vtkWindowToImageFilter.h"
#include "vtkPNMWriter.h"

#include "SaveViewerImage.h"

void main( int argc, char *argv[] )
{
 int i,j,k;

 vtkImageReader *reader = vtkImageReader::New();
 reader->SetDataByteOrderToLittleEndian();
 reader->SetDataExtent(0,255,0,255,1,93);
 reader->SetFilePrefix("../../../vtkdata/fullHead/headsq");
 reader->SetDataMask(0x7fff);

 // create exporter
 vtkImageExport *exporter = vtkImageExport::New();
 exporter->SetInput(reader->GetOutput());
 exporter->ImageLowerLeftOn();

 // get info from exporter and create array to hold data
 int memsize = exporter->GetDataMemorySize();
 int *dimensions = exporter->GetDataDimensions();
 short *data = new short[memsize/sizeof(short)];

 // this is just to improve the regression testing,
 // don't use it in a 'real' program
 exporter->GetInput()->SetMemoryLimit(512);

 // export the data into the array
 exporter->Export(data);

 /* alternative method for getting data
 short *data = exporter->GetPointerToData();
 */

 // do a little something to the data

 for (i = 0; i < dimensions[2]; i++)
   {
   for (j = 0; j < dimensions[1]; j++)
     {
     for (k = 0; k < dimensions[0]; k++)
       {
       if (k % 10 == 0)
	 {
	 data[k + dimensions[0]*(j + dimensions[1]*i)] = 0;
	 }
       if (j % 10 == 0)
	 {
	 data[k + dimensions[0]*(j + dimensions[1]*i)] = 1000;
	 }
       }
     }
   }

 // create an importer to read the data back in
 vtkImageImport *importer = vtkImageImport::New();
 importer->SetDataExtent(1,dimensions[0],1,dimensions[1],1,dimensions[2]);
 importer->ImageLowerLeftOn();
 importer->SetDataScalarTypeToShort();
 importer->SetImportVoidPointer(data);


 vtkImageViewer *viewer = vtkImageViewer::New();
 viewer->SetInput(importer->GetOutput());
 viewer->SetZSlice(128);
 viewer->SetColorWindow(2000);
 viewer->SetColorLevel(1000);

 viewer->Render();

 SAVEVIEWERIMAGE (viewer);

 char *dummy;
 cin >> dummy;
 
 viewer->Delete();
 importer->Delete();
 exporter->Delete();
 reader->Delete();
 
 delete data;

}
