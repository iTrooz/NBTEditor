#include "MainForm.h"
#include "NBTTreeModel.h"
#include "Exception/NBTException.h"
#include <iostream>
#include <QFileDialog>
#include <QShortcut>
#include <QMimeData>
#include <QMessageBox>
#include "NBT/NBTReader.h"
#include "NBT/NBTHelper.h"
#include "File/GzipByteReader.h"
#include "File/MemoryByteWriter.h"
#include "File/FileByteWriter.h"
#include "File/GzipByteWriter.h"
#include "File/WriteBuffer.h"
#include "AboutDialog.h"
#include <fstream>
#include <filesystem>

using namespace std;

namespace UI {
	MainForm::MainForm() : currentFileType(NBT::NbtGzipCompressed), saveEnabled(false) {
		widget.setupUi(this);
		setAcceptDrops(true);

		connect(widget.actionNew, SIGNAL(triggered()), this, SLOT(onActionNew()));
		connect(widget.actionOpen, SIGNAL(triggered()), this, SLOT(onActionOpen()));
		connect(widget.actionSave, SIGNAL(triggered()), this, SLOT(onActionSave()));
		connect(widget.actionSaveAs, SIGNAL(triggered()), this, SLOT(onActionSaveAs()));
		connect(widget.actionAbout, SIGNAL(triggered()), this, SLOT(onActionAbout()));

		// Add strg + s (save) shortcut
		QShortcut* saveShortcut = new QShortcut(QKeySequence("Ctrl+S"), this);
		connect(saveShortcut, SIGNAL(activated()), this, SLOT(onActionSave()));

		// Add strg + o (open) shortcut
		QShortcut* openShortcut = new QShortcut(QKeySequence("Ctrl+O"), this);
		connect(openShortcut, SIGNAL(activated()), this, SLOT(onActionOpen()));

		// Add strg + n (new) shortcut
		QShortcut* newShortcut = new QShortcut(QKeySequence("Ctrl+N"), this);
		connect(newShortcut, SIGNAL(activated()), this, SLOT(onActionNew()));

		widget.actionNew->setIcon(QIcon::fromTheme("document-new", style()->standardIcon(QStyle::SP_FileIcon)));
		widget.actionOpen->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
		widget.actionSave->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
		widget.actionSaveAs->setIcon(QIcon::fromTheme("document-save-as", style()->standardIcon(QStyle::SP_DialogSaveButton)));

		contextMenu = new TreeContextMenu(this);
		disableSaving();
	}

	MainForm::~MainForm() {
		delete contextMenu;
		contextMenu = NULL;
	}

	void MainForm::loadFile(QString path) {
		QFile file(path);
		file.open(QIODevice::ReadOnly);
		QByteArray data = file.readAll();
		loadFileContent(file.fileName(), data);
		file.close();
	}

	void MainForm::loadFileContent(const QString &filename, const QByteArray &fileContent) {
		NBT::NBTFileType oldFileType = currentFileType;
		try {
			NBT::NBTCompound* compound;
			auto data = reinterpret_cast<Byte*>(const_cast<char*>(fileContent.data()));
			if (filename.endsWith(".mca")) {
				compound = NBT::NBTReader::LoadRegionData(data, fileContent.size());
				currentFileType = NBT::NbtAnvilRegion;
			} else {
				compound = NBT::NBTReader::LoadFromData(data, fileContent.size(), &currentFileType);
			}

			NBT::NBTEntry* rootEntry = new NBT::NBTEntry("", NBT::NbtCompound);
			rootEntry->value = compound;

			UI::NBTTreeModel* newModel = new UI::NBTTreeModel(rootEntry);
			connect(newModel, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&)), this, SLOT(onTreeViewDataChanged()));

			QAbstractItemModel* oldModel = widget.treeView->model();
			widget.treeView->setModel(newModel);
			widget.treeView->expandToDepth(0);

			currentFile = filename;
			if (oldModel != NULL)
				delete oldModel;
			disableSaving();
		} catch (const Exception::Exception& ex) {
			currentFileType = oldFileType;
			QMessageBox::critical(this, tr("Can't load file"), tr("The NBT file could not be loaded:\n%1").arg(QString(ex.what())), QMessageBox::Ok, QMessageBox::Ok);
			return;
		}
	}

	void MainForm::saveFile(QString filePath) {
		try {
			NBTTreeModel* model = static_cast<NBTTreeModel*>(widget.treeView->model());
			NBT::NBTEntry* rootEntry = model->GetRootEntry();
			NBT::NBTCompound* compound = NBT::NBTHelper::GetCompound(*rootEntry);

			#ifdef __EMSCRIPTEN__
			File::MemoryByteWriter byteWriter;
			#else
			File::FileByteWriter byteWriter(filePath.toStdString());
			#endif

			File::WriteBuffer writer(&byteWriter);
			switch (currentFileType) {
				case NBT::NbtUncompressed:
					NBT::NBTReader::SaveToWriterUncompressed(byteWriter, compound);
					break;
				case NBT::NbtGzipCompressed:
					NBT::NBTReader::SaveToWriter(byteWriter, compound);
					break;
				case NBT::NbtAnvilRegion:
					NBT::NBTReader::SaveRegionToWriter(byteWriter, compound);
					break;
			}

			#ifdef __EMSCRIPTEN__
			QByteArray qByteArray(reinterpret_cast<const char*>(byteWriter.GetBuffer()), byteWriter.GetBufferSize());
			std::string filename = std::filesystem::path(filePath.toStdString()).filename();
			QFileDialog::saveFileContent(qByteArray, QString::fromStdString(filename));
			#endif

			currentFile = filePath;
			disableSaving();
		} catch (const Exception::Exception& ex) {
			QMessageBox::critical(this, tr("Can't save file"), tr("The NBT data could not be saved:\n%1").arg(QString(ex.what())), QMessageBox::Ok, QMessageBox::Ok);
			return;
		}
	}

	void MainForm::dropEvent(QDropEvent* event) {
		std::cout << "Mouse drop" << std::endl;
		const QMimeData* mimeData = event->mimeData();
		if (!mimeData->hasUrls())
			return;

		QList<QUrl> urlList = mimeData->urls();
		if (urlList.size() == 1) {
			QString file = urlList.at(0).toLocalFile();
			if (file.isEmpty())
				return;

			if (widget.treeView->model() != NULL) {
				int ret = QMessageBox::information(this, tr("Are you sure?"), tr("Do you really want to load a new file? All previous changes will be lost."), QMessageBox::No | QMessageBox::Yes, QMessageBox::Yes);
				if (ret != QMessageBox::Yes)
					return;
			}
			loadFile(file);
		}
	}

	void MainForm::onActionNew() {
		std::cout << "Action click: New" << std::endl;

		if (widget.treeView->model() != NULL) {
			int ret = QMessageBox::information(this, tr("Are you sure?"), tr("Do you really want to create a new NBT document? All previous changes will be lost."), QMessageBox::No | QMessageBox::Yes, QMessageBox::Yes);
			if (ret != QMessageBox::Yes)
				return;
		}

		NBT::NBTCompound* childCompound = new NBT::NBTCompound();
		NBT::NBTEntry* childEntry = new NBT::NBTEntry("Data", NBT::NbtCompound);
		childEntry->value = childCompound;

		NBT::NBTCompound* rootCompound = new NBT::NBTCompound();
		rootCompound->AddEntry(childEntry);
		NBT::NBTEntry* rootEntry = new NBT::NBTEntry("", NBT::NbtCompound);
		rootEntry->value = rootCompound;

		UI::NBTTreeModel* newModel = new UI::NBTTreeModel(rootEntry);
		connect(newModel, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&)), this, SLOT(onTreeViewDataChanged()));

		QAbstractItemModel* oldModel = widget.treeView->model();
		widget.treeView->setModel(newModel);
		widget.treeView->expandToDepth(1);

		currentFileType = NBT::NbtGzipCompressed;
		currentFile.clear();
		if (oldModel != NULL)
			delete oldModel;
		disableSaving();
	}

	void MainForm::onActionOpen() {
		std::cout << "Action click: Open" << std::endl;

		if (widget.treeView->model() != NULL) {
			int ret = QMessageBox::information(this, tr("Are you sure?"), tr("Do you really want to load a new file? All previous changes will be lost."), QMessageBox::No | QMessageBox::Yes, QMessageBox::Yes);
			if (ret != QMessageBox::Yes)
				return;
		}

		QFileDialog::getOpenFileContent(tr("NBT File (*.dat *.mca);;All Files (*)"), [this](const QString &fileName, const QByteArray &fileContent){
			this->loadFileContent(fileName, fileContent);
		});
	}

	void MainForm::onActionSave() {
		std::cout << "On Action: Save" << std::endl;
		if (!saveEnabled || widget.treeView->model() == NULL)
			return;

		if (currentFile.isEmpty()) {
			onActionSaveAs();
			return;
		}

		saveFile(currentFile);
	}

	void MainForm::onActionSaveAs() {
		std::cout << "On Action: Save As" << std::endl;
		// TODO: Disable button instead of just block the action
		if (widget.treeView->model() == NULL)
			return;
		
		#ifdef __EMSCRIPTEN__
		saveFile("");
		#else
		QString filePath = QFileDialog::getSaveFileName(this, tr("Save file"), "", tr("NBT File (*.dat);;All Files (*)"));
		if (filePath.isNull())
			return;

		saveFile(filePath);
		#endif
	}

	void MainForm::onActionAbout() {
		AboutDialog dialog(this);
		dialog.exec();
	}

	void MainForm::onTreeViewDataChanged() {
		if (!saveEnabled)
			enableSaving();
	}

	void MainForm::enableSaving() {
		GetWidget().actionSave->setEnabled(true);
		saveEnabled = true;
	}

	void MainForm::disableSaving() {
		GetWidget().actionSave->setEnabled(false);
		saveEnabled = false;
	}

}