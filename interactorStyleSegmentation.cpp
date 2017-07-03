#include "interactorStyleSegmentation.h"

vtkStandardNewMacro(InteractorStyleSegmentation);

void InteractorStyleSegmentation::OnLeftButtonDown() {
	if (viewer != NULL && slicePlane != NULL && this->GetDefaultRenderer() != NULL) {
		vtkSmartPointer<vtkVolumePicker> picker = vtkSmartPointer<vtkVolumePicker>::New();
		int* pos = this->GetInteractor()->GetEventPosition();
		picker->Pick(pos[0], pos[1], pos[2], this->GetDefaultRenderer());

		int* ijk = picker->GetPointIJK();
		
		if (picker->GetPointId() != -1) {
			ijk[2] = slicePlane->getPlane()->GetCenter()[2] / sculpture->getImageData()->GetSpacing()[2];
			int * dimensions = sculpture->getImageData()->GetDimensions();
			Bounds bounds;
			bounds.MIN_X = 0;
			bounds.MAX_X = dimensions[0];
			bounds.MIN_Y = 0;
			bounds.MAX_Y = dimensions[1];
			bounds.MIN_Z = 0;
			bounds.MAX_Z = dimensions[2];

			std::vector<std::vector<Line> > lines(bounds.MAX_Z);

			lines[ijk[2]] = getLinesFromImage(sculpture->getImageData(), sculpture->getTransferFunction()->getColorFun(), ijk[2], bounds);

			std::string img = generateImage(sculpture->getImageData(), sculpture->getTransferFunction()->getColorFun(), ijk[2], bounds, lines[ijk[2]]);

			// -- launch line selection
			LineSelectionDialog *diag = new LineSelectionDialog();
			diag->setImage(img);
			// -- END launch line selection

			remove(img.c_str());

			// -- exec line selection
			int response = diag->exec();
			// -- END exec line selection

			if (response != LINE_CANCEL) {
				Line selectedLine;
				switch (response) {
					case LINE_RED:
						selectedLine = lines[ijk[2]][0];
						break;
					case LINE_GREEN:
						selectedLine = lines[ijk[2]][1];
						break;
					case LINE_BLUE:
						selectedLine = lines[ijk[2]][2];
						break;
					case LINE_CYAN:
						selectedLine = lines[ijk[2]][3];
						break;
					case LINE_MAGENTA:
						selectedLine = lines[ijk[2]][4];
						break;
					case LINE_YELLOW:
						selectedLine = lines[ijk[2]][5];
						break;
				}

				// -- launch progress bar
				QPointer<QProgressBar> bar = new QProgressBar(0);
				QPointer<QProgressDialog> progressDialog = new QProgressDialog(0);
				progressDialog->setWindowTitle(QString("Segmentando..."));
				progressDialog->setLabelText(QString::fromLatin1("Segmentando la pieza de madera seleccionada"));
				progressDialog->setWindowIcon(QIcon(":/icons/3DCurator.png"));
				progressDialog->setWindowFlags(progressDialog->windowFlags() & ~Qt::WindowCloseButtonHint);
				progressDialog->setCancelButton(0);
				progressDialog->setBar(bar);
				progressDialog->show();
				bar->close();
				QApplication::processEvents();
				// -- END launch progress bar

				regionGrowingWithLineBoundVolume(sculpture->getImageData(), sculpture->getTransferFunction()->getColorFun(), ijk, bounds, selectedLine, lines);

				sculpture->getImageData()->Modified();
				slicePlane->getPlane()->UpdatePlacement();
				viewer->Render();

				// -- close progress bar
				progressDialog->close();
				// -- END close progress bar
			}
		}
	}
}

void InteractorStyleSegmentation::SetViewer(vtkSmartPointer<vtkImageViewer2> viewer) {
	this->viewer = viewer;
}

void InteractorStyleSegmentation::SetSlicePlane(SlicePlane* slicePlane) {
	this->slicePlane = slicePlane;
}

void InteractorStyleSegmentation::SetSculpture(Sculpture* sculpture) {
	this->sculpture = sculpture;
}