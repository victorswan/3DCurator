#include "MainWindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
	ui->setupUi(this);
	ui->isoValueSlider->setTracking(false); // do not launch slider event until we release it

	deleting = false;
	showPlane = true;
	segmentating = false;

	// initiate counters
	sliceRuleCounter = 0;

	itemListEnabled = QFont();
	itemListDisabled = QFont();
	itemListDisabled.setItalic(true);

	volumeRen = vtkSmartPointer<vtkRenderer>::New();
	meshRen = vtkSmartPointer<vtkRenderer>::New();
	sliceViewer = vtkSmartPointer<vtkImageViewer2>::New();
	volumeStyle = vtkSmartPointer<vtkInteractorStyleTrackballCamera>::New();
	sliceStyle = vtkSmartPointer<InteractorStyleImage>::New();
	deleterStyle = vtkSmartPointer<InteractorStyleDeleter>::New();
	segmentationStyle = vtkSmartPointer<InteractorStyleSegmentation>::New();
	slicePlane = new SlicePlane();
	sculpture = new Sculpture();
	defaultTF(); 
	defaultMaterial();
	defaultBackgroundsColors();

	colorTFChart = new ColorTFChart(ui->volumeWidget->GetRenderWindow(), ui->colorTFWidget->GetRenderWindow(), sculpture->getTransferFunction()->getColorFun(), "Densidad", "", MIN_INTENSITY, MAX_INTENSITY);
	scalarTFChart = new OpacityTFChart(ui->volumeWidget->GetRenderWindow(), ui->scalarTFWidget->GetRenderWindow(), sculpture->getTransferFunction()->getScalarFun(), "Densidad", "Opacidad", MIN_INTENSITY, MAX_INTENSITY);
	gradientTFChart = new OpacityTFChart(ui->volumeWidget->GetRenderWindow(), ui->gradientTFWidget->GetRenderWindow(), sculpture->getTransferFunction()->getGradientFun(), "Gradiente", "Opacidad", 0, MAX_INTENSITY - MIN_INTENSITY);
	updateSliders();

	sliceViewer->GetWindowLevel()->SetLookupTable(sculpture->getTransferFunction()->getColorFun());
	sliceViewer->SetColorLevel(-600);
	sliceViewer->SetColorWindow(400);

	setBackgroundColor(volumeRen, volumeBackground.redF(), volumeBackground.greenF(), volumeBackground.blueF());
	setBackgroundColor(meshRen, meshBackground.redF(), meshBackground.greenF(), meshBackground.blueF());

	connectComponents();

	renderVolume();
	renderMesh();

	slicePlane->show(false);
	enablePlane();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::setBackgroundColor(vtkSmartPointer<vtkRenderer> ren, float r, float g, float b) {
	ren->SetBackground(r, g, b);
}

void MainWindow::connectComponents() {
	ui->volumeWidget->GetRenderWindow()->AddRenderer(volumeRen);
	ui->meshWidget->GetRenderWindow()->AddRenderer(meshRen);

	ui->slicesWidget->SetRenderWindow(sliceViewer->GetRenderWindow());
	sliceViewer->SetInputData(slicePlane->getPlane()->GetResliceOutput()); 

	ui->volumeWidget->GetRenderWindow()->GetInteractor()->SetInteractorStyle(volumeStyle);
	slicePlane->getPlane()->SetInteractor(ui->volumeWidget->GetRenderWindow()->GetInteractor());

	sliceViewer->SetupInteractor(ui->slicesWidget->GetInteractor());
	sliceStyle->SetSlicePlane(slicePlane);
	sliceStyle->SetDefaultRenderer(sliceViewer->GetRenderer());
	sliceStyle->SetLabel(ui->coordsAndValueLabel);
	ui->slicesWidget->GetInteractor()->SetInteractorStyle(sliceStyle);

	slicePlane->setViewer(sliceViewer);

	deleterStyle->SetSculpture(sculpture);
	deleterStyle->SetSlicePlane(slicePlane);
	deleterStyle->SetDefaultRenderer(volumeRen);
	deleterStyle->SetViewer(sliceViewer);
	deleterStyle->SetDefaultRenderWindow(ui->volumeWidget->GetRenderWindow());

	segmentationStyle->SetSculpture(sculpture);
	segmentationStyle->SetSlicePlane(slicePlane);
	segmentationStyle->SetViewer(sliceViewer);
	segmentationStyle->SetDefaultRenderer(sliceViewer->GetRenderer());
}

void MainWindow::drawVolume() {
	volumeRen->AddViewProp(sculpture->getVolume());
	volumeRen->ResetCamera();
	renderVolume();
}

void MainWindow::drawMesh() {
	meshRen->AddActor(sculpture->getMesh());
	meshRen->ResetCamera();
	renderMesh();
}

void MainWindow::removeVolume() {
	volumeRen->RemoveAllViewProps();
	volumeRen->ResetCamera();
	renderVolume();
}

void MainWindow::removeMesh() {
	meshRen->RemoveAllViewProps();
	meshRen->ResetCamera();
	renderMesh();
}

void MainWindow::renderVolume() {
	ui->volumeWidget->GetRenderWindow()->Render();
}

void MainWindow::renderMesh() {
	ui->meshWidget->GetRenderWindow()->Render();
}

void MainWindow::renderSlice() {
	sliceViewer->Render();
}

void MainWindow::loadDefaultPreset(QFile *file) {
	if (file->open(QIODevice::ReadOnly | QIODevice::Text)) {
		std::istringstream ss;
		ss.str(QString(file->readAll()).toStdString()); //Get istringstream from QFile

		sculpture->getTransferFunction()->read(ss);
		file->close(); 

		// Update tags
		ui->tfName->setText(QString::fromUtf8(sculpture->getTransferFunction()->getName().c_str()));
		ui->tfDescription->setText(QString::fromUtf8(sculpture->getTransferFunction()->getDescription().c_str()));
	} else {
		cerr << "Error abriendo archivo por defecto de funci�n de transferencia" << endl;
		exit(-1);
	}
}

void MainWindow::defaultTF() {
	QFile file(":/presets/ct-woodsculpture.xml"); 
	loadDefaultPreset(&file);
}

void MainWindow::defaultMaterial() {
	ui->ambientValue->setValue(0.1); 
	ui->diffuseValue->setValue(0.9); 
	ui->specularValue->setValue(0.2);
	ui->powerValue->setValue(10.0);
}

void MainWindow::defaultBackgroundsColors() {
	volumeBackground = QColor::fromRgbF(0.1, 0.2, 0.3);
	volumeDeletingBackground = QColor::fromRgbF(0.2, 0.3, 0.1);
	meshBackground = QColor::fromRgbF(0.1, 0.2, 0.3);

	ui->volumeBackground->setStyleSheet("background-color: " + volumeBackground.name());
	ui->volumeDeletingBackground->setStyleSheet("background-color: " + volumeDeletingBackground.name());
	ui->meshBackground->setStyleSheet("background-color: " + meshBackground.name());
}

void MainWindow::defaultPlanePosition() {
	if (sculpture->getVolume() != NULL) { 
		// volume dimension
		double xSize = sculpture->getMaxXBound() - sculpture->getMinXBound();
		double ySize = sculpture->getMaxYBound() - sculpture->getMinYBound();
		double zSize = sculpture->getMaxZBound() - sculpture->getMinZBound();
		slicePlane->setOrigin(xSize / 2, ySize / 2, zSize / 2); // plane in the center of the sculpture
		slicePlane->setAxial();
	} else {
		launchWarningNoVolume();
	}
}

void MainWindow::updateMaterial() {
	sculpture->setMaterial(ui->ambientValue->value(), ui->diffuseValue->value(), ui->specularValue->value(), ui->powerValue->value());
}

void MainWindow::updateSliders() {
	ui->colorTFMinSlider->setMinimum((int)MIN_INTENSITY);
	ui->colorTFMinSlider->setValue((int)sculpture->getTransferFunction()->getColorFun()->GetRange()[0]);
	ui->colorTFMinSlider->setMaximum((int)sculpture->getTransferFunction()->getColorFun()->GetRange()[1] - 1);
	ui->colorTFMaxSlider->setMinimum((int)sculpture->getTransferFunction()->getColorFun()->GetRange()[0] + 1);
	ui->colorTFMaxSlider->setValue((int)sculpture->getTransferFunction()->getColorFun()->GetRange()[1]);
	ui->colorTFMaxSlider->setMaximum((int)MAX_INTENSITY);


	ui->scalarTFMinSlider->setMinimum((int)MIN_INTENSITY);
	ui->scalarTFMinSlider->setValue((int)sculpture->getTransferFunction()->getScalarFun()->GetRange()[0]);
	ui->scalarTFMinSlider->setMaximum((int)sculpture->getTransferFunction()->getScalarFun()->GetRange()[1] - 1);
	ui->scalarTFMaxSlider->setMinimum((int)sculpture->getTransferFunction()->getScalarFun()->GetRange()[0] + 1);
	ui->scalarTFMaxSlider->setValue((int)sculpture->getTransferFunction()->getScalarFun()->GetRange()[1]);
	ui->scalarTFMaxSlider->setMaximum((int)MAX_INTENSITY);

	ui->gradientTFMinSlider->setMinimum(0);
	ui->gradientTFMinSlider->setValue((int)sculpture->getTransferFunction()->getGradientFun()->GetRange()[0]);
	ui->gradientTFMinSlider->setMaximum((int)sculpture->getTransferFunction()->getGradientFun()->GetRange()[1] - 1);
	ui->gradientTFMaxSlider->setMinimum((int)sculpture->getTransferFunction()->getGradientFun()->GetRange()[0] + 1);
	ui->gradientTFMaxSlider->setValue((int)sculpture->getTransferFunction()->getGradientFun()->GetRange()[1]);
	ui->gradientTFMaxSlider->setMaximum((int)MAX_INTENSITY);
}

void MainWindow::importDICOM() {
	QString dicomFolder = QFileDialog::getExistingDirectory(this, tr("Abrir carpeta DICOM"), QDir::homePath(), QFileDialog::ShowDirsOnly); 

	if (dicomFolder != NULL) {
		// -- launch progress bar
		QPointer<QProgressBar> bar = new QProgressBar(0);
		QPointer<QProgressDialog> progressDialog = new QProgressDialog(0);
		progressDialog->setWindowTitle(QString("Cargando..."));
		progressDialog->setLabelText(QString::fromLatin1("Cargando los datos DICOM especificados"));
		progressDialog->setWindowIcon(QIcon(":/icons/3DCurator.png"));
		progressDialog->setWindowFlags(progressDialog->windowFlags() & ~Qt::WindowCloseButtonHint);
		progressDialog->setCancelButton(0);
		progressDialog->setBar(bar);
		progressDialog->show();
		bar->close();
		QApplication::processEvents();
		// -- END launch progress bar

		removeVolume();
		removeMesh();
		clearAllRules(); 

		slicePlane->show(false);
		disablePlane();
		sculpture->setDICOMFolder(dicomFolder.toUtf8().constData());
		slicePlane->setInputData(sculpture->getImageData());
		ui->labelFolder->setText(dicomFolder);
		defaultPlanePosition();
		slicePlane->show(true);
		enablePlane();

		drawVolume();
		drawMesh();
		renderSlice();

		// -- close progress bar
		progressDialog->close();
		// -- END close progress bar
	}
}

void MainWindow::importPreset() {
	QString presetFile = QFileDialog::getOpenFileName(this, tr("Importar preset"), QDir::homePath());

	if (presetFile != NULL) {
		std::string s = presetFile.toUtf8().constData();
		sculpture->getTransferFunction()->read(s);

		// update tags
		ui->tfName->setText(QString::fromUtf8(sculpture->getTransferFunction()->getName().c_str()));
		ui->tfDescription->setText(QString::fromUtf8(sculpture->getTransferFunction()->getDescription().c_str()));

		colorTFChart->defaultRange(); 
		scalarTFChart->defaultRange();
		gradientTFChart->defaultRange();

		updateSliders();
	}
}

void MainWindow::exportImageFromRenderWindow(vtkSmartPointer<vtkRenderWindow> renWin, const QString filename) {
	if (filename != NULL) {
		vtkSmartPointer<vtkWindowToImageFilter> filter = vtkSmartPointer<vtkWindowToImageFilter>::New(); 
		filter->SetInput(renWin);
		filter->Update();
		vtkSmartPointer<vtkImageWriter> writer;
		if (toUpper(getFileExtension(filename.toUtf8().constData())) == "PNG") {
			writer = vtkSmartPointer<vtkPNGWriter>::New();
		} else {
			writer = vtkSmartPointer<vtkJPEGWriter>::New();
		}
		writer->SetFileName(filename.toUtf8().constData()); 
		writer->SetInputConnection(filter->GetOutputPort());
		writer->Write();
	}
}

void MainWindow::exportMeshToFile(const QString filename) {
	if (filename != NULL) { 
		// -- launch progress bar
		QPointer<QProgressBar> bar = new QProgressBar(0);
		QPointer<QProgressDialog> progressDialog = new QProgressDialog(0);
		progressDialog->setWindowTitle(QString("Extrayendo..."));
		progressDialog->setLabelText(QString::fromLatin1("Extrayendo la malla del modelo"));
		progressDialog->setWindowIcon(QIcon(":/icons/3DCurator.png"));
		progressDialog->setWindowFlags(progressDialog->windowFlags() & ~Qt::WindowCloseButtonHint);
		progressDialog->setCancelButton(0);
		progressDialog->setBar(bar);
		progressDialog->show();
		bar->close();
		QApplication::processEvents();
		// -- END launch progress bar

		// marching cubes
		vtkSmartPointer<vtkMarchingCubes> surface = vtkSmartPointer<vtkMarchingCubes>::New();
		surface->SetInputData(sculpture->getImageData()); 
		surface->UpdateInformation(); 
		surface->ComputeScalarsOn();
		surface->ComputeGradientsOn();
		surface->ComputeNormalsOn();
		surface->SetValue(0, sculpture->getIsoValue()); 

		// get mesh
		vtkSmartPointer<vtkPolyData> marched = vtkSmartPointer<vtkPolyData>::New();
		surface->Update(); 
		marched->DeepCopy(surface->GetOutput());

		// mesh reduction
		vtkSmartPointer<vtkDecimatePro> decimator = vtkSmartPointer<vtkDecimatePro>::New();
		decimator->SetInputData(marched); 
		decimator->SetTargetReduction(0.5); 
		decimator->SetPreserveTopology(true);
		decimator->Update();

		// mesh smooth
		vtkSmartPointer<vtkSmoothPolyDataFilter> smoother = vtkSmartPointer<vtkSmoothPolyDataFilter>::New();
		smoother->SetInputData(decimator->GetOutput());
		smoother->SetNumberOfIterations(5);
		smoother->SetFeatureAngle(60);
		smoother->SetRelaxationFactor(0.05);
		smoother->FeatureEdgeSmoothingOff();
		smoother->Update();

		// get final mesh
		vtkSmartPointer<vtkPolyData> mesh = vtkSmartPointer<vtkPolyData>::New();
		mesh->ShallowCopy(smoother->GetOutput());

		// export to STL
		vtkSmartPointer<vtkSTLWriter> stlWriter = vtkSmartPointer<vtkSTLWriter>::New();
		stlWriter->SetFileName(filename.toUtf8().constData());
		stlWriter->SetInputData(mesh);
		stlWriter->Write();

		// -- close progress bar
		progressDialog->close();
		// -- END close progress bar
	}
}

void MainWindow::exportPreset(const QString filename) {
	if (filename != NULL) {
		// get name and description from GUI
		sculpture->getTransferFunction()->setName(ui->tfName->text().toUtf8().constData());
		sculpture->getTransferFunction()->setDescription(ui->tfDescription->text().toUtf8().constData());

		std::string s = filename.toUtf8().constData();
		sculpture->getTransferFunction()->write(s);
	}
}

QString MainWindow::getExportPresetFilename(const QString defaultFilename) {
	return QFileDialog::getSaveFileName(this, tr("Exportar preset"), QDir(QDir::homePath()).filePath(defaultFilename), "XML (*.xml)");
}

QString MainWindow::getExportImageFilename(const QString defaultFilename) {
	return QFileDialog::getSaveFileName(this, tr("Exportar imagen"), QDir(QDir::homePath()).filePath(defaultFilename), "PNG (*.png);;JPG (*.jpg)");
}

QString MainWindow::getExportMeshFilename(const QString defaultFilename) {
	return QFileDialog::getSaveFileName(this, tr("Exportar malla"), QDir(QDir::homePath()).filePath(defaultFilename), "STL (*.stl)");
}

void MainWindow::enablePlane() {
	slicePlane->enable(true);
	QIcon icon(":/icons/eye-slash.png");
	ui->enableDisablePlane->setIcon(icon); 
	showPlane = true; 
}

void MainWindow::disablePlane() {
	slicePlane->enable(false);
	QIcon icon(":/icons/eye.png");
	ui->enableDisablePlane->setIcon(icon);
	showPlane = false;
}

void MainWindow::exportMesh() {
	if (sculpture->getLoaded()) {
		exportMeshToFile(getExportMeshFilename("Mesh"));
	} else {
		launchWarningNoVolume();
	}
}

void MainWindow::updateMesh() {
	if (sculpture->getLoaded()) { 
		// -- launch progress bar
		QPointer<QProgressBar> bar = new QProgressBar(0);
		QPointer<QProgressDialog> progressDialog = new QProgressDialog(0);
		progressDialog->setWindowTitle(QString("Actualizando..."));
		progressDialog->setLabelText(QString::fromLatin1("Generando el modelo con la isosuperficie especificada"));
		progressDialog->setWindowIcon(QIcon(":/icons/3DCurator.png"));
		progressDialog->setWindowFlags(progressDialog->windowFlags() & ~Qt::WindowCloseButtonHint);
		progressDialog->setCancelButton(0);
		progressDialog->setBar(bar);
		progressDialog->show();
		bar->close();
		QApplication::processEvents();
		// -- END launch progress bar

		sculpture->createMesh();
		meshRen->Render();

		// -- close progress bar
		progressDialog->close();
		// -- END close progress bar

	} else {
		launchWarningNoVolume();
	}
}

void MainWindow::enableDisablePlane() {
	if (showPlane) {
		disablePlane();
	} else {
		enablePlane();
	}
	renderVolume();
}

void MainWindow::axialPlane() {
	if (sculpture->getLoaded()) {
		slicePlane->setAxial();
		renderVolume();
		renderSlice();
	} else {
		launchWarningNoVolume();
	}
}

void MainWindow::coronalPlane() {
	if (sculpture->getLoaded()) {
		slicePlane->setCoronal();
		renderVolume();
		renderSlice();
	} else {
		launchWarningNoVolume();
	}
}

void MainWindow::sagitalPlane() {
	if (sculpture->getLoaded()) {
		slicePlane->setSagital();
		renderVolume();
		renderSlice();
	} else {
		launchWarningNoVolume();
	}
}

void MainWindow::deleteVolumeParts() {
	if (deleting) {
		deleting = false;
		setBackgroundColor(volumeRen, volumeBackground.redF(), volumeBackground.greenF(), volumeBackground.blueF());
		ui->volumeWidget->GetRenderWindow()->GetInteractor()->SetInteractorStyle(volumeStyle);
	} else {
		deleting = true;
		setBackgroundColor(volumeRen, volumeDeletingBackground.redF(), volumeDeletingBackground.greenF(), volumeDeletingBackground.blueF());
		ui->volumeWidget->GetRenderWindow()->GetInteractor()->SetInteractorStyle(deleterStyle);
	}
	slicePlane->getPlane()->UpdatePlacement();
	renderVolume();
	renderSlice();
}

void MainWindow::importCompletePreset() {
	QFile file(":/presets/ct-woodsculpture.xml");
	loadDefaultPreset(&file);
	renderVolume();
}

void MainWindow::importWoodPreset() {
	QFile file(":/presets/ct-onlywood.xml");
	loadDefaultPreset(&file);
	renderVolume();
}

void MainWindow::importStuccoPreset() {
	QFile file(":/presets/ct-onlystucco.xml");
	loadDefaultPreset(&file);
	renderVolume();
}

void MainWindow::importMetalPreset() {
	QFile file(":/presets/ct-onlymetal.xml");
	loadDefaultPreset(&file);
	renderVolume();
}

void MainWindow::addRule() {
	if (rules.size() < RULES_LIMIT) {
		std::string id;
		QListWidgetItem *item = new QListWidgetItem(0); // create GUI item
		sliceRuleCounter++; 
		id = "Regla " + std::to_string(sliceRuleCounter); 
		item->setText(id.c_str()); 
		item->setFont(itemListEnabled); 
		ui->ruleList->addItem(item);
		ui->ruleList->setCurrentItem(item);
		rules[item] = vtkSmartPointer<vtkDistanceWidget>::New(); 
		rules[item]->SetInteractor(ui->slicesWidget->GetInteractor());
		rules[item]->CreateDefaultRepresentation(); 
		static_cast<vtkDistanceRepresentation *>(rules[item]->GetRepresentation())->SetLabelFormat("%-#6.3g mm"); 
		rules[item]->On();
	} else {
		launchWarningTooManyRules();
	}
}

void MainWindow::deleteRule() {
	if (ui->ruleList->currentItem() != NULL) { 
		rules.erase(ui->ruleList->currentItem()); 
		delete ui->ruleList->currentItem(); 
		renderSlice();
		if (ui->ruleList->count() == 0) {
			sliceRuleCounter = 0;
		}
	} else {
		launchWarningNoRule();
	}
}

void MainWindow::enableDisableRule() {
	if (ui->ruleList->currentItem() != NULL) {
		if (ui->ruleList->currentItem()->font() == itemListDisabled) {
			enableRule();
			ui->ruleList->currentItem()->setFont(itemListEnabled);
		} else {
			disableRule();
			ui->ruleList->currentItem()->setFont(itemListDisabled);
		}
	} else {
		launchWarningNoRule();
	}
}

void MainWindow::enableRule() {
	if (ui->ruleList->currentItem() != NULL) {
		rules[ui->ruleList->currentItem()]->On();
	} else {
		launchWarningNoRule();
	}
}

void MainWindow::disableRule() {
	if (ui->ruleList->currentItem() != NULL) {
		rules[ui->ruleList->currentItem()]->Off();
	} else {
		launchWarningNoRule();
	}
}

void MainWindow::clearAllRules() {
	rules.clear(); // clear container

	// clear GUI lists
	ui->ruleList->clear();

	// reset counters
	sliceRuleCounter = 0;
}

void MainWindow::changeBackgroundColor(const int widget) {
	QColor color;
	switch (widget) {
		case VOLUME_BACKGROUND:
			color = QColorDialog::getColor(volumeBackground);
			if (color.isValid()) {
				volumeBackground = color;
				if (!deleting) { 
					setBackgroundColor(volumeRen, volumeBackground.redF(), volumeBackground.greenF(), volumeBackground.blueF());
					renderVolume(); 
				}
				ui->volumeBackground->setStyleSheet("background-color: " + volumeBackground.name()); 
			}
			break;
		case VOLUME_DELETING_BACKGROUND: 
			color = QColorDialog::getColor(volumeDeletingBackground);
			if (color.isValid()) {
				volumeDeletingBackground = color;
				if (deleting) {
					setBackgroundColor(volumeRen, volumeDeletingBackground.redF(), volumeDeletingBackground.greenF(), volumeDeletingBackground.blueF());
					renderVolume();
				}
				ui->volumeDeletingBackground->setStyleSheet("background-color: " + volumeDeletingBackground.name());
			}
			break;
		case MESH_BACKGROUND:
			color = QColorDialog::getColor(meshBackground);
			if (color.isValid()) {
				meshBackground = color;
				setBackgroundColor(meshRen, meshBackground.redF(), meshBackground.greenF(), meshBackground.blueF());
				renderMesh();
				ui->meshBackground->setStyleSheet("background-color: " + meshBackground.name());
			}
			break;
	}
}

void MainWindow::restoreBackgroundsColors() {
	defaultBackgroundsColors();
	if (deleting) {
		setBackgroundColor(volumeRen, volumeDeletingBackground.redF(), volumeDeletingBackground.greenF(), volumeDeletingBackground.blueF());
	} else { 
		setBackgroundColor(volumeRen, volumeBackground.redF(), volumeBackground.greenF(), volumeBackground.blueF());
	}
	setBackgroundColor(meshRen, meshBackground.redF(), meshBackground.greenF(), meshBackground.blueF());
	renderVolume();
	renderMesh();
}
void MainWindow::launchWarning(const std::string message) {
	QPointer<QMessageBox> confirmBox = new QMessageBox(0);
	confirmBox->setWindowTitle(QString::fromLatin1("Advertencia"));
	confirmBox->setWindowIcon(QIcon(":/icons/3DCurator.png"));
	confirmBox->setIcon(QMessageBox::Information);
	confirmBox->setText(QString::fromLatin1(message.c_str()));
	confirmBox->setStandardButtons(QMessageBox::Ok);
	confirmBox->exec();
}

void MainWindow::launchWarningNoVolume() {
	launchWarning("Hace falta cargar un modelo antes");
}

void MainWindow::launchWarningNoRule() {
	launchWarning("Seleccione una regla antes");
}

void MainWindow::launchWarningTooManyRules() {
	launchWarning("Se ha alcanzado el m�ximo n�mero de reglas permitidas");
}

void MainWindow::segmentateOnOff() {
	if (segmentating) {
		ui->slicesWidget->GetRenderWindow()->GetInteractor()->SetInteractorStyle(sliceStyle);
		ui->segmentate->setIcon(QIcon(":/icons/scissors.png"));
		segmentating = false;
	} else {
		ui->slicesWidget->GetRenderWindow()->GetInteractor()->SetInteractorStyle(segmentationStyle); 
		ui->segmentate->setIcon(QIcon(":/icons/scissors-slash.png"));
		segmentating = true;
	}
}

void MainWindow::filter() {
	if (sculpture->getLoaded()) {
		// -- launch progress bar
		QPointer<QProgressBar> bar = new QProgressBar(0);
		QPointer<QProgressDialog> progressDialog = new QProgressDialog(0);
		progressDialog->setWindowTitle(QString("Filtrando..."));
		progressDialog->setLabelText(QString::fromLatin1("Aplicando un filtro Gaussiano para reducir el ruido"));
		progressDialog->setWindowIcon(QIcon(":/icons/3DCurator.png"));
		progressDialog->setWindowFlags(progressDialog->windowFlags() & ~Qt::WindowCloseButtonHint);
		progressDialog->setCancelButton(0);
		progressDialog->setBar(bar);
		progressDialog->show();
		bar->close();
		QApplication::processEvents();
		// -- END launch progress bar

		sculpture->filter();

		// -- close progress bar
		progressDialog->close();
		// -- END close progress bar
	} else {
		launchWarningNoVolume();
	}
}

//---------------------------------------------------------------------------------------------------------------------------------
// GUI Events - Menu
//---------------------------------------------------------------------------------------------------------------------------------

void MainWindow::on_actionOpenDICOM_triggered() {
	importDICOM();
}

void MainWindow::on_actionExportVolumeImage_triggered() {
	exportImageFromRenderWindow(ui->volumeWidget->GetRenderWindow(), getExportImageFilename(QString::fromStdString(getCurrentDate())));
}

void MainWindow::on_actionExportSliceImage_triggered() {
	exportImageFromRenderWindow(ui->slicesWidget->GetRenderWindow(), getExportImageFilename(QString::fromStdString(getCurrentDate())));
}

void MainWindow::on_actionExit_triggered() {
	QPointer<QMessageBox> confirmBox = new QMessageBox(0);
	confirmBox->setWindowTitle(QString::fromLatin1("Advertencia"));
	confirmBox->setWindowIcon(QIcon(":/icons/3DCurator.png"));
	confirmBox->setIcon(QMessageBox::Information);
	confirmBox->setText(QString::fromLatin1("�Seguro que desea salir?"));
	confirmBox->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
	confirmBox->button(QMessageBox::Yes)->setText(QString::fromLatin1("S�"));
	confirmBox->button(QMessageBox::No)->setText(QString::fromLatin1("No"));
	if (confirmBox->exec() == QMessageBox::Yes) {
		exit(0);
	}
}

void MainWindow::on_actionEnableDisablePlane_triggered() {
	enableDisablePlane();
}

void MainWindow::on_actionSagitalPlane_triggered() {
	sagitalPlane();
}

void MainWindow::on_actionAxialPlane_triggered() {
	axialPlane();
}

void MainWindow::on_actionCoronalPlane_triggered() {
	coronalPlane();
}

void MainWindow::on_actionImportPreset_triggered() {
	importPreset();
}

void MainWindow::on_actionCompletePreset_triggered() {
	importCompletePreset();
}

void MainWindow::on_actionWoodPreset_triggered() {
	importWoodPreset();
}

void MainWindow::on_actionStuccoPreset_triggered() {
	importStuccoPreset();
}

void MainWindow::on_actionMetalPreset_triggered() {
	importMetalPreset();
}

void MainWindow::on_actionExportPreset_triggered() {
	exportPreset(getExportPresetFilename(ui->tfName->text()));
}

void MainWindow::on_actionDeleteVolumeParts_triggered() {
	deleteVolumeParts();
}

void MainWindow::on_actionExtractMesh_triggered() {
	exportMesh();
}

void MainWindow::on_actionWoodMesh_triggered() {
	ui->isoValueSlider->setValue(WOOD_ISOVALUE);
}

void MainWindow::on_actionStuccoMesh_triggered() {
	ui->isoValueSlider->setValue(STUCCO_ISOVALUE);
}

void MainWindow::on_actionMetalMesh_triggered() {
	ui->isoValueSlider->setValue(METAL_ISOVALUE);
}

void MainWindow::on_actionFilter_triggered() {
	filter();
}

//---------------------------------------------------------------------------------------------------------------------------------
// GUI Events - Buttons
//---------------------------------------------------------------------------------------------------------------------------------

void MainWindow::on_openDICOM_pressed() {
	importDICOM();
}

void MainWindow::on_axialPlane_pressed() {
	axialPlane();
}

void MainWindow::on_coronalPlane_pressed() {
	coronalPlane();
}

void MainWindow::on_sagitalPlane_pressed() {
	sagitalPlane();
}

void MainWindow::on_exportSliceImage_pressed() {
	exportImageFromRenderWindow(ui->slicesWidget->GetRenderWindow(), getExportImageFilename(QString::fromStdString(getCurrentDate())));
}

void MainWindow::on_exportVolumeImage_pressed() {
	exportImageFromRenderWindow(ui->volumeWidget->GetRenderWindow(), getExportImageFilename(QString::fromStdString(getCurrentDate())));
}

void MainWindow::on_importPreset_pressed() {
	importPreset();
}

void MainWindow::on_exportPreset_pressed() {
	exportPreset(getExportPresetFilename(ui->tfName->text()));
}

void MainWindow::on_restoreMaterial_pressed() {
	defaultMaterial();
}

void MainWindow::on_updateProperties_pressed() {
	updateMaterial();
	renderVolume();
}

void MainWindow::on_completePreset_pressed() {
	importCompletePreset();
}

void MainWindow::on_woodPreset_pressed() {
	importWoodPreset();
}

void MainWindow::on_stuccoPreset_pressed() {
	importStuccoPreset();
}

void MainWindow::on_metalPreset_pressed() {
	importMetalPreset();
}

void MainWindow::on_extractMesh_pressed() {
	exportMesh();
}

void MainWindow::on_extractMeshWood_pressed() {
	ui->isoValueSlider->setValue(WOOD_ISOVALUE);
}

void MainWindow::on_extractMeshStucco_pressed() {
	ui->isoValueSlider->setValue(STUCCO_ISOVALUE);
}

void MainWindow::on_extractMeshMetal_pressed() {
	ui->isoValueSlider->setValue(METAL_ISOVALUE);
}

void MainWindow::on_enableDisablePlane_pressed() {
	enableDisablePlane();
}

void MainWindow::on_deleteVolumeParts_pressed() {
	deleteVolumeParts();
}

void MainWindow::on_addRule_pressed() {
	addRule();
}

void MainWindow::on_deleteRule_pressed() {
	deleteRule();
}

void MainWindow::on_enableDisableRule_pressed() {
	enableDisableRule();
}

void MainWindow::on_volumeBackground_pressed() {
	changeBackgroundColor(0);
}

void MainWindow::on_volumeDeletingBackground_pressed() {
	changeBackgroundColor(1);
}

void MainWindow::on_meshBackground_pressed() {
	changeBackgroundColor(2);
}

void MainWindow::on_restoreBackgrounds_pressed() {
	restoreBackgroundsColors();
}

void MainWindow::on_segmentate_pressed() {
	segmentateOnOff();
}

void MainWindow::on_filter_pressed() {
	filter();
}

//---------------------------------------------------------------------------------------------------------------------------------
// GUI Events - Sliders
//---------------------------------------------------------------------------------------------------------------------------------

void MainWindow::on_colorTFMaxSlider_valueChanged() {
	colorTFChart->setRange((double)ui->colorTFMinSlider->value(), (double)ui->colorTFMaxSlider->value());
	ui->colorTFMinSlider->setMaximum(ui->colorTFMaxSlider->value() - 1);
}

void MainWindow::on_colorTFMinSlider_valueChanged() {
	colorTFChart->setRange((double)ui->colorTFMinSlider->value(), (double)ui->colorTFMaxSlider->value());
	ui->colorTFMaxSlider->setMinimum(ui->colorTFMinSlider->value() + 1);
}

void MainWindow::on_gradientTFMaxSlider_valueChanged() {
	gradientTFChart->setRange((double)ui->gradientTFMinSlider->value(), (double)ui->gradientTFMaxSlider->value());
	ui->gradientTFMinSlider->setMaximum(ui->gradientTFMaxSlider->value() - 1);
}

void MainWindow::on_gradientTFMinSlider_valueChanged() {
	gradientTFChart->setRange((double)ui->gradientTFMinSlider->value(), (double)ui->gradientTFMaxSlider->value());
	ui->gradientTFMaxSlider->setMinimum(ui->gradientTFMinSlider->value() + 1);
}

void MainWindow::on_scalarTFMaxSlider_valueChanged() {
	scalarTFChart->setRange((double)ui->scalarTFMinSlider->value(), (double)ui->scalarTFMaxSlider->value());
	ui->scalarTFMinSlider->setMaximum(ui->scalarTFMaxSlider->value() - 1);
}

void MainWindow::on_scalarTFMinSlider_valueChanged() {
	scalarTFChart->setRange((double)ui->scalarTFMinSlider->value(), (double)ui->scalarTFMaxSlider->value());
	ui->scalarTFMaxSlider->setMinimum(ui->scalarTFMinSlider->value() + 1);
}

void MainWindow::on_isoValueSlider_valueChanged() {
	sculpture->setIsoValue(ui->isoValueSlider->value());
	updateMesh();
}