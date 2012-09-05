/* -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*- */
// $Id$
/*
 * SysEx Librarian Window
 *
 * ==========================================================================
 *
 *  Copyright (C) 2012 Thorsten Klose (tk@midibox.org)
 *  Licensed for personal non-commercial use only.
 *  All other rights reserved.
 * 
 * ==========================================================================
 */

#include "SysexLibrarian.h"
#include "MiosStudio.h"

SysexLibrarianBank::SysexLibrarianBank(MiosStudio *_miosStudio)
    : miosStudio(_miosStudio)
    , font(14.0f)
{
    addAndMakeVisible(table = new TableListBox(T("Bank"), this));
    table->setColour(ListBox::outlineColourId, Colours::grey);
    table->setOutlineThickness(1);
    table->setMultipleSelectionEnabled(true);

    table->getHeader().addColumn(T("Patch"), 1, 50);
    table->getHeader().addColumn(T("Name"), 2, 140);

    initBank(0);

    setSize(120, 200);
}

SysexLibrarianBank::~SysexLibrarianBank()
{
}

//==============================================================================
int SysexLibrarianBank::getNumRows()
{
    return numRows;
}

void SysexLibrarianBank::setNumRows(const int& rows)
{
    numRows = rows;

    patchStorage.clear();
    patchName.clear();
    for(int i=0; i<numRows; ++i) {
        patchName.set(i, T(""));
    }
}


void SysexLibrarianBank::initBank(const unsigned& _patchSpec)
{
    if( _patchSpec < miosStudio->sysexPatchDb->getNumSpecs() ) {
        patchSpec = _patchSpec;

        // this workaround ensures that the table view will be updated
        numRows = 10001;
        table->selectRow(10000);

        setNumRows(miosStudio->sysexPatchDb->getNumPatchesPerBank(patchSpec));
        table->selectRow(0);
    }
}

unsigned SysexLibrarianBank::getSelectedPatch(void)
{
    return table->getSelectedRow();
}

bool SysexLibrarianBank::isSelectedPatch(const unsigned& patch)
{
    return table->isRowSelected(patch);
}

void SysexLibrarianBank::selectPatch(const unsigned& patch)
{
    table->selectRow(patch);
}

void SysexLibrarianBank::paintRowBackground(Graphics &g, int rowNumber, int width, int height, bool rowIsSelected)
{
    if( rowIsSelected )
        g.fillAll(Colours::lightblue);
}

void SysexLibrarianBank::paintCell(Graphics &g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    g.setColour(Colours::black);
    g.setFont(font);

    switch( columnId ) {
    case 1: {
        char buffer[20];
        sprintf(buffer, "%03d", rowNumber+1);
        const String text(buffer);
        g.drawText(text, 2, 0, width - 4, height, Justification::centred, true);
    } break;
    }

    g.setColour(Colours::black.withAlpha (0.2f));
    g.fillRect(width - 1, 0, 1, height);
}


Component* SysexLibrarianBank::refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, Component* existingComponentToUpdate)
{
    switch( columnId ) {
    case 2: {
        ConfigTableLabel *label = (ConfigTableLabel *)existingComponentToUpdate;

        if( label == 0 ) {
            label = new ConfigTableLabel(*this);
        }

        label->setRowAndColumn(rowNumber, columnId);
        return label;
    } break;
    }

    return 0;
}


//==============================================================================
void SysexLibrarianBank::resized()
{
    // position our table with a gap around its edge
    table->setBoundsInset(BorderSize(8));
}


//==============================================================================
int SysexLibrarianBank::getTableValue(const int rowNumber, const int columnId)
{
    return 0;
}

void SysexLibrarianBank::setTableValue(const int rowNumber, const int columnId, const int newValue)
{
}


//==============================================================================
String SysexLibrarianBank::getTableString(const int rowNumber, const int columnId)
{
    switch( columnId ) {
    case 2: return patchName[rowNumber];
    }
    return String(T("???"));
}

void SysexLibrarianBank::setTableString(const int rowNumber, const int columnId, const String newString)
{
    switch( columnId ) {
    case 2: {
        Array<uint8>* p = patchStorage[rowNumber];
        if( p != NULL && p->size() ) {
            // replace patchname
            miosStudio->sysexPatchDb->replacePatchNameInPayload(patchSpec, *p, newString);

            // and back from payload (to doublecheck that it has been taken over)
            patchName.set(rowNumber, miosStudio->sysexPatchDb->getPatchNameFromPayload(patchSpec, *p));
        }
    } break;
    }
}


//==============================================================================
Array<uint8>* SysexLibrarianBank::getPatch(const uint8& patch)
{
    return patchStorage[patch];
}

void SysexLibrarianBank::setPatch(const uint8& patch, const Array<uint8> &payload)
{
    if( patchStorage[patch] != NULL ) {
        //juce_free(patchStorage[patch]);
    }
    patchStorage.set(patch, new Array<uint8>(payload));
    patchName.set(patch, miosStudio->sysexPatchDb->getPatchNameFromPayload(patchSpec, payload));

    table->resized(); // will do the trick, repaint() doesn't cause update
}


//==============================================================================
//==============================================================================
//==============================================================================
SysexLibrarianControl::SysexLibrarianControl(MiosStudio *_miosStudio, SysexLibrarian *_sysexLibrarian)
    : miosStudio(_miosStudio)
    , sysexLibrarian(_sysexLibrarian)
    , currentPatch(0)
    , progress(0)
    , receiveDump(false)
    , handleSinglePatch(false)
    , bufferTransfer(false)
    , dumpReceived(false)
    , checksumError(false)
    , errorResponse(false)
{
    addAndMakeVisible(deviceTypeLabel = new Label(T("MIDI Device:"), T("MIDI Device:")));
    deviceTypeLabel->setJustificationType(Justification::right);

	addAndMakeVisible(deviceTypeSelector = new ComboBox(String::empty));
	deviceTypeSelector->addListener(this);
	deviceTypeSelector->clear();
    for(int i=0; i<miosStudio->sysexPatchDb->getNumSpecs(); ++i) {
        deviceTypeSelector->addItem(miosStudio->sysexPatchDb->getSpecName(i), i+1);
    }
    deviceTypeSelector->setSelectedId(1, true);
    deviceTypeSelector->setEnabled(true);

    addAndMakeVisible(deviceIdLabel = new Label(T("Device ID"), T("Device ID:")));
    deviceIdLabel->setJustificationType(Justification::right);

    addAndMakeVisible(deviceIdSlider = new Slider(T("Device ID")));
    deviceIdSlider->setRange(0, 7, 1);
    deviceIdSlider->setSliderStyle(Slider::IncDecButtons);
    deviceIdSlider->setTextBoxStyle(Slider::TextBoxLeft, false, 30, 20);
    deviceIdSlider->setDoubleClickReturnValue(true, 0);
    
    addAndMakeVisible(bankSelectLabel = new Label(T("Bank"), T("Bank:")));
    bankSelectLabel->setJustificationType(Justification::right);

    addAndMakeVisible(bankSelectSlider = new Slider(T("Bank")));
    bankSelectSlider->setRange(1, 8, 1);
    bankSelectSlider->setSliderStyle(Slider::IncDecButtons);
    bankSelectSlider->setTextBoxStyle(Slider::TextBoxLeft, false, 30, 20);
    bankSelectSlider->setDoubleClickReturnValue(true, 0);
    
    addAndMakeVisible(loadBankButton = new TextButton(T("Load Bank")));
    loadBankButton->addListener(this);
    addAndMakeVisible(saveBankButton = new TextButton(T("Save Bank")));
    saveBankButton->addListener(this);
    addAndMakeVisible(receiveBankButton = new TextButton(T("Receive Bank")));
    receiveBankButton->addListener(this);
    addAndMakeVisible(sendBankButton = new TextButton(T("Send Bank")));
    sendBankButton->addListener(this);

    addAndMakeVisible(loadPatchButton = new TextButton(T("Load Patch")));
    loadPatchButton->addListener(this);
    addAndMakeVisible(savePatchButton = new TextButton(T("Save Patch")));
    savePatchButton->addListener(this);
    addAndMakeVisible(receivePatchButton = new TextButton(T("Receive Patch")));
    receivePatchButton->addListener(this);
    addAndMakeVisible(sendPatchButton = new TextButton(T("Send Patch")));
    sendPatchButton->addListener(this);

    addAndMakeVisible(bufferLabel = new Label(T("Buffer"), T("SID:")));
    bufferLabel->setJustificationType(Justification::right);

    addAndMakeVisible(bufferSlider = new Slider(T("Buffer")));
    bufferSlider->setRange(1, 4, 1);
    bufferSlider->setSliderStyle(Slider::IncDecButtons);
    bufferSlider->setTextBoxStyle(Slider::TextBoxLeft, false, 30, 20);
    bufferSlider->setDoubleClickReturnValue(true, 0);

    addAndMakeVisible(receiveBufferButton = new TextButton(T("Receive Buffer")));
    receiveBufferButton->addListener(this);
    addAndMakeVisible(sendBufferButton = new TextButton(T("Send Buffer")));
    sendBufferButton->addListener(this);

    addAndMakeVisible(stopButton = new TextButton(T("Stop")));
    stopButton->addListener(this);

    addAndMakeVisible(progressBar = new ProgressBar(progress));

    // restore settings
    PropertiesFile *propertiesFile = ApplicationProperties::getInstance()->getCommonSettings(true);
    if( propertiesFile ) {
        deviceIdSlider->setValue(propertiesFile->getIntValue(T("sysexLibrarianDeviceId"), 0) & 7);
        String syxFileName(propertiesFile->getValue(T("sysexLibrarianSyxFile"), String::empty));
        if( syxFileName != String::empty )
            syxFile = File(syxFileName);
    }

    setSize(300, 200);
}

SysexLibrarianControl::~SysexLibrarianControl()
{
}

//==============================================================================
void SysexLibrarianControl::paint (Graphics& g)
{
    g.fillAll(Colours::white);
}

void SysexLibrarianControl::resized()
{
    int buttonX0 = 15;
    int buttonXOffset = 100;
    int buttonY = 8;
    int buttonYOffset = 32;
    int buttonWidth = 90;
    int buttonHeight = 24;

#if 0
    deviceTypeLabel->setBounds   (buttonX0 + 0*buttonXOffset, buttonY + 0*buttonYOffset, buttonWidth, buttonHeight);
    deviceTypeSelector->setBounds(buttonX0 + 1*buttonXOffset, buttonY + 0*buttonYOffset, buttonWidth, buttonHeight);
#else
    deviceTypeSelector->setBounds(buttonX0 + 0*buttonXOffset, buttonY + 0*buttonYOffset, 2*buttonWidth+10, buttonHeight);
#endif

    deviceIdLabel->setBounds (buttonX0 + 0*buttonXOffset, buttonY + 1*buttonYOffset, buttonWidth, buttonHeight);
    deviceIdSlider->setBounds(buttonX0 + 1*buttonXOffset, buttonY + 1*buttonYOffset+4, buttonWidth, buttonHeight-8);

    bankSelectLabel->setBounds (buttonX0 + 0*buttonXOffset, buttonY + 3*buttonYOffset, buttonWidth, buttonHeight);
    bankSelectSlider->setBounds(buttonX0 + 1*buttonXOffset, buttonY + 3*buttonYOffset+4, buttonWidth, buttonHeight-8);

    loadBankButton->setBounds   (buttonX0 + 0*buttonXOffset, buttonY + 4*buttonYOffset, buttonWidth, buttonHeight);
    saveBankButton->setBounds   (buttonX0 + 1*buttonXOffset, buttonY + 4*buttonYOffset, buttonWidth, buttonHeight);
    receiveBankButton->setBounds(buttonX0 + 0*buttonXOffset, buttonY + 5*buttonYOffset, buttonWidth, buttonHeight);
    sendBankButton->setBounds   (buttonX0 + 1*buttonXOffset, buttonY + 5*buttonYOffset, buttonWidth, buttonHeight);

    loadPatchButton->setBounds   (buttonX0 + 0*buttonXOffset, buttonY + 7*buttonYOffset, buttonWidth, buttonHeight);
    savePatchButton->setBounds   (buttonX0 + 1*buttonXOffset, buttonY + 7*buttonYOffset, buttonWidth, buttonHeight);
    receivePatchButton->setBounds(buttonX0 + 0*buttonXOffset, buttonY + 8*buttonYOffset, buttonWidth, buttonHeight);
    sendPatchButton->setBounds   (buttonX0 + 1*buttonXOffset, buttonY + 8*buttonYOffset, buttonWidth, buttonHeight);

    bufferLabel->setBounds (buttonX0 + 0*buttonXOffset, buttonY + 10*buttonYOffset, buttonWidth, buttonHeight);
    bufferSlider->setBounds(buttonX0 + 1*buttonXOffset, buttonY + 10*buttonYOffset+4, buttonWidth, buttonHeight-8);

    receiveBufferButton->setBounds(buttonX0 + 0*buttonXOffset, buttonY + 11*buttonYOffset, buttonWidth, buttonHeight);
    sendBufferButton->setBounds   (buttonX0 + 1*buttonXOffset, buttonY + 11*buttonYOffset, buttonWidth, buttonHeight);

    progressBar->setBounds(buttonX0 + 0*buttonXOffset + 45 + 10, getHeight()-buttonY-buttonHeight, (buttonWidth-45-10)+buttonWidth+10, buttonHeight);
    stopButton->setBounds(buttonX0 + 0*buttonXOffset, getHeight()-buttonY-buttonHeight, 45, buttonHeight);
}

//==============================================================================
void SysexLibrarianControl::buttonClicked(Button* buttonThatWasClicked)
{
    if( buttonThatWasClicked == stopButton ) {
        stopTransfer();
    } else if( buttonThatWasClicked == loadBankButton ||
               buttonThatWasClicked == loadPatchButton ) {
        FileChooser fc(T("Choose a .syx file that you want to open..."),
                       syxFile,
                       T("*.syx"));

        if( fc.browseForFileToOpen() ) {
            syxFile = fc.getResult();
            if( loadSyx(syxFile, buttonThatWasClicked == loadBankButton) ) {
                PropertiesFile *propertiesFile = ApplicationProperties::getInstance()->getCommonSettings(true);
                if( propertiesFile )
                    propertiesFile->setValue(T("sysexLibrarianSyxFile"), syxFile.getFullPathName());
            }
        }
    } else if( buttonThatWasClicked == saveBankButton ||
               buttonThatWasClicked == savePatchButton ) {
        FileChooser fc(T("Choose a .syx file that you want to save..."),
                       syxFile,
                       T("*.syx"));
        if( fc.browseForFileToSave(true) ) {
            syxFile = fc.getResult();
            if( saveSyx(syxFile, buttonThatWasClicked == saveBankButton) ) {
                PropertiesFile *propertiesFile = ApplicationProperties::getInstance()->getCommonSettings(true);
                if( propertiesFile )
                    propertiesFile->setValue(T("sysexLibrarianSyxFile"), syxFile.getFullPathName());
            }
        }
    } else if( buttonThatWasClicked == sendBankButton ||
               buttonThatWasClicked == receiveBankButton ||
               buttonThatWasClicked == sendPatchButton ||
               buttonThatWasClicked == receivePatchButton ||
               buttonThatWasClicked == sendBufferButton ||
               buttonThatWasClicked == receiveBufferButton ) {
        deviceIdSlider->setEnabled(false);
        bankSelectSlider->setEnabled(false);
        loadBankButton->setEnabled(false);
        saveBankButton->setEnabled(false);
        sendBankButton->setEnabled(false);
        receiveBankButton->setEnabled(false);
        loadPatchButton->setEnabled(false);
        savePatchButton->setEnabled(false);
        sendPatchButton->setEnabled(false);
        receivePatchButton->setEnabled(false);
        bufferSlider->setEnabled(false);
        sendBufferButton->setEnabled(false);
        receiveBufferButton->setEnabled(false);
        stopButton->setEnabled(true);

        currentPatch = (buttonThatWasClicked == sendBankButton || buttonThatWasClicked == receiveBankButton) ? 0 : sysexLibrarian->sysexLibrarianBank->getSelectedPatch();
        progress = 0;
        errorResponse = false;
        checksumError = false;

        handleSinglePatch =
            buttonThatWasClicked == sendPatchButton ||
            buttonThatWasClicked == receivePatchButton ||
            buttonThatWasClicked == sendBufferButton ||
            buttonThatWasClicked == receiveBufferButton;

        bufferTransfer =
            buttonThatWasClicked == sendBufferButton ||
            buttonThatWasClicked == receiveBufferButton;

        receiveDump =
            buttonThatWasClicked == receiveBankButton ||
            buttonThatWasClicked == receivePatchButton ||
            buttonThatWasClicked == receiveBufferButton;

        startTimer(1);
    }
}


//==============================================================================
void SysexLibrarianControl::sliderValueChanged(Slider* slider)
{
    if( slider == deviceIdSlider ) {
        // store setting
        PropertiesFile *propertiesFile = ApplicationProperties::getInstance()->getCommonSettings(true);
        if( propertiesFile )
            propertiesFile->setValue(T("sysexLibrarianDeviceId"), (int)slider->getValue());
    }
}


//==============================================================================
void SysexLibrarianControl::comboBoxChanged(ComboBox* comboBox)
{
    if( comboBox == deviceTypeSelector ) {
        int spec = comboBox->getSelectedId()-1;
        if( spec >= 0 && spec < miosStudio->sysexPatchDb->getNumSpecs() ) {
            sysexLibrarian->sysexLibrarianBank->initBank(spec);
            bankSelectSlider->setRange(1, miosStudio->sysexPatchDb->getNumBanks(spec), 1);
        }
    }
}

//==============================================================================

void SysexLibrarianControl::stopTransfer(void)
{
    stopTimer();

    progress = 0;
    deviceIdSlider->setEnabled(true);
    bankSelectSlider->setEnabled(true);
    loadBankButton->setEnabled(true);
    saveBankButton->setEnabled(true);
    sendBankButton->setEnabled(true);
    receiveBankButton->setEnabled(true);
    loadPatchButton->setEnabled(true);
    savePatchButton->setEnabled(true);
    sendPatchButton->setEnabled(true);
    receivePatchButton->setEnabled(true);
    bufferSlider->setEnabled(true);
    sendBufferButton->setEnabled(true);
    receiveBufferButton->setEnabled(true);
    stopButton->setEnabled(false);
}

//==============================================================================
void SysexLibrarianControl::timerCallback()
{
    stopTimer(); // will be restarted if required

    // transfer has been stopped?
    if( !stopButton->isEnabled() )
        return;

    bool transferFinished = false;

    if( receiveDump ) {
        if( currentPatch > 0 ) {
            if( !dumpReceived ) {
                transferFinished = true;
                AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                            T("No response from core."),
                                            T("Check:\n- MIDI In/Out connections\n- Device ID\n- that MIDIbox firmware has been uploaded"),
                                            String::empty);
            } else if( checksumError ) {
                transferFinished = true;
                AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                            T("Detected checksum error!"),
                                            T("Check:\n- MIDI In/Out connections\n- your MIDI interface"),
                                            String::empty);
            }
        }

        if( !transferFinished ) {
            int spec = deviceTypeSelector->getSelectedId()-1;
            if( spec < 0 || spec >= miosStudio->sysexPatchDb->getNumSpecs() ) {
                transferFinished = true;
            } else if( (handleSinglePatch && currentPatch != sysexLibrarian->sysexLibrarianBank->getSelectedPatch()) ||
                       (currentPatch >= miosStudio->sysexPatchDb->getNumPatchesPerBank(spec)) ) {
                transferFinished = true;
            } else {
                dumpReceived = false;
                checksumError = false;
                errorResponse = false;
                int spec = deviceTypeSelector->getSelectedId()-1;
                if( spec < 0 || spec >= miosStudio->sysexPatchDb->getNumSpecs() ) {
                    dumpReceived = true;
                } else {
                    Array<uint8> data;

                    if( bufferTransfer ) {
                        data = miosStudio->sysexPatchDb->createReadBuffer(spec,
                                                                         (uint8)deviceIdSlider->getValue(),
                                                                         0,
                                                                         (uint8)bankSelectSlider->getValue()-1,
                                                                         currentPatch);
                    } else {
                        data = miosStudio->sysexPatchDb->createReadPatch(spec,
                                                                         (uint8)deviceIdSlider->getValue(),
                                                                         0,
                                                                         (uint8)bankSelectSlider->getValue()-1,
                                                                         currentPatch);
                    }
                    MidiMessage message = SysexHelper::createMidiMessage(data);
                    miosStudio->sendMidiMessage(message);

                    ++currentPatch;

                    if( handleSinglePatch )
                        progress = 1;
                    else
                        progress = (double)currentPatch / (double)miosStudio->sysexPatchDb->getNumPatchesPerBank(spec);
                    startTimer(miosStudio->sysexPatchDb->getDelayBetweenReads(spec));
                }
            }
        }
    } else {
        int spec = deviceTypeSelector->getSelectedId()-1;
        if( spec < 0 || spec >= miosStudio->sysexPatchDb->getNumSpecs() ) {
            transferFinished = true;
        } else {
            if( errorResponse ) {
                transferFinished = true;
                AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                            T("Got Error response!"),
                                            T("Check:\n- if a valid patch has been uploaded\n- error code in MIDI IN monitor"),
                                            String::empty);
            } else {
                if( (handleSinglePatch && currentPatch != sysexLibrarian->sysexLibrarianBank->getSelectedPatch()) ||
                    currentPatch >= miosStudio->sysexPatchDb->getNumPatchesPerBank(spec) ) {
                    transferFinished = true;
                } else {
                    Array<uint8>* p = NULL;
                    while( currentPatch < sysexLibrarian->sysexLibrarianBank->getNumRows() &&
                           (p=sysexLibrarian->sysexLibrarianBank->getPatch(currentPatch)) == NULL )
                        ++currentPatch;

                    if( p == NULL ) {
                        transferFinished = true;
                    } else {
                        errorResponse = false;

                        Array<uint8> data;

                        if( bufferTransfer ) {
                            data = miosStudio->sysexPatchDb->createWriteBuffer(spec,
                                                                              (uint8)deviceIdSlider->getValue(),
                                                                              0,
                                                                              (uint8)bankSelectSlider->getValue()-1,
                                                                              currentPatch,
                                                                              &p->getReference(0),
                                                                              p->size());
                        } else {
                            data = miosStudio->sysexPatchDb->createWritePatch(spec,
                                                                              (uint8)deviceIdSlider->getValue(),
                                                                              0,
                                                                              (uint8)bankSelectSlider->getValue()-1,
                                                                              currentPatch,
                                                                              &p->getReference(0),
                                                                              p->size());
                        }

                        MidiMessage message = SysexHelper::createMidiMessage(data);
                        miosStudio->sendMidiMessage(message);

                        ++currentPatch;

                        if( handleSinglePatch )
                            progress = 1;
                        else
                            progress = (double)currentPatch / (double)miosStudio->sysexPatchDb->getNumPatchesPerBank(spec);

                        startTimer(miosStudio->sysexPatchDb->getDelayBetweenWrites(spec));
                    }
                }
            }
        }
    }

    if( transferFinished ) {
        stopTransfer();
    }
}

//==============================================================================
void SysexLibrarianControl::handleIncomingMidiMessage(const MidiMessage& message, uint8 runningStatus)
{
    uint8 *data = message.getRawData();
    uint32 size = message.getRawDataSize();

    int spec = deviceTypeSelector->getSelectedId()-1;
    if( spec < 0 || spec >= miosStudio->sysexPatchDb->getNumSpecs() )
        return;

    if( receiveDump ) {
        int receivedPatch = currentPatch-1;
        if( (bufferTransfer && miosStudio->sysexPatchDb->isValidWriteBuffer(spec,
                                                                            data, size,
                                                                            (int)deviceIdSlider->getValue(),
                                                                            0,
                                                                            -1,
                                                                            -1))
            ||
            (!bufferTransfer && miosStudio->sysexPatchDb->isValidWritePatch(spec,
                                                                            data, size,
                                                                            (int)deviceIdSlider->getValue(),
                                                                            0,
                                                                            bankSelectSlider->getValue()-1,
                                                                            receivedPatch))
            ) {

            dumpReceived = true;

            if( size != miosStudio->sysexPatchDb->getPatchSize(spec) ||
                !miosStudio->sysexPatchDb->hasValidChecksum(spec, data, size) ) {
                checksumError = true;
            } else {
                Array<uint8> payload(miosStudio->sysexPatchDb->getPayload(spec, data, size));
                sysexLibrarian->sysexLibrarianBank->setPatch(receivedPatch, payload);
                sysexLibrarian->sysexLibrarianBank->selectPatch(receivedPatch);
            }

            // trigger timer immediately
            stopTimer();
            startTimer(1);
        }
    } else if( isTimerRunning() ) {
        if( miosStudio->sysexPatchDb->isValidErrorAcknowledge(spec, data, size, (int)deviceIdSlider->getValue()) ) {
            // trigger timer immediately
            errorResponse = true;
            stopTimer();
            startTimer(1);
        } else if( miosStudio->sysexPatchDb->isValidAcknowledge(spec, data, size, (int)deviceIdSlider->getValue()) ) {
            // trigger timer immediately
            stopTimer();
            startTimer(1);
        }
    }
}


//==============================================================================
bool SysexLibrarianControl::loadSyx(File &syxFile, const bool& loadBank)
{
    FileInputStream *inFileStream = syxFile.createInputStream();

    if( !inFileStream || inFileStream->isExhausted() || !inFileStream->getTotalLength() ) {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                    T("The file ") + syxFile.getFileName(),
                                    T("doesn't exist!"),
                                    String::empty);
        return false;
    } else if( inFileStream->isExhausted() || !inFileStream->getTotalLength() ) {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                    T("The file ") + syxFile.getFileName(),
                                    T("is empty!"),
                                    String::empty);
        return false;
    }

    int64 size = inFileStream->getTotalLength();
    uint8 *buffer = (uint8 *)juce_malloc(size);
    int64 readNumBytes = inFileStream->read(buffer, size);
    String errorMessage;
    int spec = deviceTypeSelector->getSelectedId()-1;

    if( readNumBytes != size ) {
        errorMessage = String(T("cannot be read completely!"));
    } else if( spec < 0 || spec >= miosStudio->sysexPatchDb->getNumSpecs() ) {
        errorMessage = String(T("invalid patch type selected!"));
    } else {
        int patchSize = miosStudio->sysexPatchDb->getPatchSize(spec);
        int maxPatches = miosStudio->sysexPatchDb->getNumPatchesPerBank(spec);

        if( loadBank ) {
            sysexLibrarian->sysexLibrarianBank->initBank(spec);
        }

        unsigned patch = 0;
        unsigned numPatches = 0;
        int pos = 0;
        while( errorMessage.isEmpty() &&
               (pos+patchSize) <= size ) {
            uint8* patchPtr = &buffer[pos];
            if( miosStudio->sysexPatchDb->isValidWritePatch(spec, patchPtr, patchSize, -1, -1, -1, -1) ||
                miosStudio->sysexPatchDb->isValidWriteBuffer(spec, patchPtr, patchSize, -1, -1, -1, -1) ) {

                if( !loadBank ) {
                    while( patch < maxPatches && !sysexLibrarian->sysexLibrarianBank->isSelectedPatch(patch) )
                        ++patch;
                }
                if( patch >= maxPatches )
                    break;

                Array<uint8> payload(miosStudio->sysexPatchDb->getPayload(spec, patchPtr, patchSize));
                sysexLibrarian->sysexLibrarianBank->setPatch(patch, payload);
                if( loadBank )
                    sysexLibrarian->sysexLibrarianBank->selectPatch(patch);

                ++numPatches;
                ++patch;
                pos += patchSize;
            } else {
                // search for next F0
                while( ((++pos+patchSize) <= size) && (buffer[pos] != 0xf0) );
            }
        }

        if( numPatches == 0 ) {
            errorMessage = String(T("doesn't contain any valid SysEx dump for a ") + miosStudio->sysexPatchDb->getSpecName(spec));
        } else {
            if( loadBank ) {
                sysexLibrarian->sysexLibrarianBank->selectPatch(0); // select the first patch in bank
            }
        }
    }

    juce_free(buffer);

    if( !errorMessage.isEmpty() ) {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                    T("The file ") + syxFile.getFileName(),
                                    errorMessage,
                                    String::empty);
        return false;
    }

    return true;
}

bool SysexLibrarianControl::saveSyx(File &syxFile, const bool& saveBank)
{
    syxFile.deleteFile();
    FileOutputStream *outFileStream = syxFile.createOutputStream();
            
    if( !outFileStream || outFileStream->failedToOpen() ) {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                    String::empty,
                                    T("File cannot be created!"),
                                    String::empty);
        return false;
    }


    int spec = deviceTypeSelector->getSelectedId()-1;
    if( spec < 0 || spec >= miosStudio->sysexPatchDb->getNumSpecs() ) {
        AlertWindow::showMessageBox(AlertWindow::WarningIcon,
                                    String::empty,
                                    T("Invalid patch type selected!"),
                                    String::empty);
    } else {
        int maxPatches = sysexLibrarian->sysexLibrarianBank->getNumRows();
        for(int patch=0; patch<maxPatches; ++patch) {
            if( saveBank || sysexLibrarian->sysexLibrarianBank->isSelectedPatch(patch) ) {
                Array<uint8>* p = NULL;
                if( ((p=sysexLibrarian->sysexLibrarianBank->getPatch(patch)) != NULL) && p->size() ) {
                    Array<uint8> syxDump(miosStudio->sysexPatchDb->createWritePatch(spec,
                                                                                    0,
                                                                                    0,
                                                                                    (uint8)bankSelectSlider->getValue()-1,
                                                                                    patch,
                                                                                    &p->getReference(0),
                                                                                    p->size()));
                    outFileStream->write(&syxDump.getReference(0), syxDump.size());
                }
            }
        }
    }

    delete outFileStream;

    return true;
}


//==============================================================================
//==============================================================================
//==============================================================================
SysexLibrarian::SysexLibrarian(MiosStudio *_miosStudio)
    : miosStudio(_miosStudio)
{
    addAndMakeVisible(sysexLibrarianBank = new SysexLibrarianBank(miosStudio));
    addAndMakeVisible(sysexLibrarianControl = new SysexLibrarianControl(miosStudio, this));

    resizeLimits.setSizeLimits(100, 300, 2048, 2048);
    addAndMakeVisible(resizer = new ResizableCornerComponent(this, &resizeLimits));

    setSize(450, 500);

    // initial bank
    unsigned spec = 0;
    sysexLibrarianBank->initBank(spec);
}

SysexLibrarian::~SysexLibrarian()
{
}

//==============================================================================
void SysexLibrarian::paint (Graphics& g)
{
    //g.fillAll(Colour(0xffc1d0ff));
    g.fillAll(Colours::white);
}

void SysexLibrarian::resized()
{
    sysexLibrarianControl->setBounds(0, 0, 225, getHeight());
    sysexLibrarianBank->setBounds(220, 0, 225, getHeight());
    resizer->setBounds(getWidth()-16, getHeight()-16, 16, 16);
}