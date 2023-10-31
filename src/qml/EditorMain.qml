import QtQuick 2.15
import QtQuick.Controls 2.15
import MarkdownHighlighter 1.0
import com.company.BlockModel 1.0
import nuttyartist.notes 1.0
import QtQuick.Controls.Universal 2.15
import QtQuick.Shapes 1.15

Rectangle {
    id: root
    width: 900
    height: 480
    color: root.themeData.backgroundColor

    property string displayFontFamily: ""
    property string platform: "Other"
    property int qtVersion: 6
    property var themeData: ({backgroundColor: "white"})
    property int currentFontPointBaseSize: 13
    property int currentFontPointSize: if (blockEditorView.width < 420) {
                                           root.currentFontPointBaseSize - 2
                                       } else if (blockEditorView.width >= 420 && blockEditorView.width <= 1250) {
                                           currentFontPointBaseSize
                                       } else { //if (blockEditorView.width > 1250) {
                                           if (root.platform === "Apple") {
                                               root.currentFontPointBaseSize + 2
                                           } else {
                                               root.currentFontPointBaseSize + 1
                                           }
                                       }
    property string currentEditorTextColor: root.themeData.theme === "Dark" ? "white" : "black"
    property int pointSizeOffset: platform === "Apple" ? 0: -3

    FontIconLoader {
        id: fontIconLoader
    }

    Connections {
        target: mainWindow

        function onDisplayFontSet (data) {
            root.displayFontFamily = data.displayFont;
        }

        function onPlatformSet (data) {
            root.platform = data;

        }

        function onQtVersionSet (data) {
            root.qtVersion = data;

        }

        function onThemeChanged (data) {
            root.themeData = data;
        }

        function onFontsChanged (data) {
            root.listOfSansSerifFonts = data.listOfSansSerifFonts;
            root.listOfSerifFonts = data.listOfSerifFonts;
            root.listOfMonoFonts = data.listOfMonoFonts;
            root.chosenSansSerifFontIndex = data.chosenSansSerifFontIndex;
            root.chosenSerifFontIndex = data.chosenSerifFontIndex;
            root.chosenMonoFontIndex = data.chosenMonoFontIndex;
            root.currentFontTypeface = data.currentFontTypeface;
            root.currentlySelectedFontFamily = root.currentFontTypeface === "SansSerif" ?
                        root.listOfSansSerifFonts[root.chosenSansSerifFontIndex] :
                        root.currentFontTypeface === "Serif" ?
                            root.listOfSerifFonts[root.chosenSerifFontIndex] :
                            root.currentFontTypeface === "Mono" ?
                                root.listOfMonoFonts[root.chosenMonoFontIndex] : "";
            root.currentFontPointBaseSize = data.currentFontPointSize;
            root.currentEditorTextColor = data.currentEditorTextColor;
        }
    }

    property int editorRightLeftPadding: if (blockEditorView.width <= 420) {
                                             18
                                         } else if (blockEditorView.width > 420 && blockEditorView.width <= 515) {
                                             43
                                         } else if (blockEditorView.width > 515 && blockEditorView.width <= 755) {
                                             53
                                         } else if (blockEditorView.width > 755 && blockEditorView.width <= 775) {
                                             63
                                         } else if (blockEditorView.width > 755 && blockEditorView.width <= 800) {
                                             73
                                         } else if (blockEditorView.width > 800) {
                                             83
                                         }
//    property int editorRightLeftPadding: 53
    property real editorWidth: root.width
    property string accentColor: "#2383e2"
    property string selectionColor: root.themeData.theme === "Dark" ? "#31353a" : "#d2e4fa" // "#e9f2fd"
    property string headingColor: root.themeData.theme === "Dark" ? "#ccdbe5" : "#444444"
    property color todoItemCheckedTextColor: root.themeData.theme === "Dark" ? "#7f7f7f" : Qt.rgba(55/255, 53/255, 47/255, 0.45)
    property list<string> listOfSansSerifFonts: []
    property list<string> listOfSerifFonts: []
    property list<string> listOfMonoFonts: []
    property int chosenSansSerifFontIndex: 0
    property int chosenSerifFontIndex: 0
    property int chosenMonoFontIndex: 0
    property string currentFontTypeface
    property string currentlySelectedFontFamily
    property bool isProgrammaticChange: false
    property int lastCursorPos: 0
    property int defaultIndentWidth: 25
    property list<int> selectedBlockIndexes: []
    property bool showBlockBorder: false
    property int blockIndexToFocusOn: -1
    property real cursorX: 0
    property bool isCursorMovedVertically: false
    property real cursorXSaved: 0
    property bool isAnyKeyPressed: false
    property bool isHoldingControl: false
    property bool isHoldingShift: false
    property bool isHoldingCapsLock: false
    property var selectedBlock: null
    property bool enableCursorAnimation: true
    property int cursorDefaultAnimationSpeed: 100
    property int cursorCurrentAnimationSpeed: 100
    property bool cursorAnimationRunning: true
    signal anyKeyPressed
    signal cursorHidden
    signal cursorShowed
    property bool canUpdateCursorPos: true
    property rect lastCursorRect: Qt.rect(0,0,0,0)
    property string currentHoveredLink: ""

    Connections {
        target: BlockModel

        function onAboutToChangeText() {
            root.isProgrammaticChange = true;
        }

        function onTextChangeFinished() {
            console.log("In onTextChangeFinished:", root.lastCursorPos);
            root.isProgrammaticChange = false;
            root.selectedBlock.textEditorPointer.cursorPosition = root.lastCursorPos;
        }

        function onAboutToLoadText() {
            root.isProgrammaticChange = true;
        }

        function onLoadTextFinished(data) {
            root.isProgrammaticChange = false;
            blockEditorView.positionViewAtIndex(data.itemIndexInView, ListView.Beginning);
        }

        function onNewBlockCreated(blockIndex : int) {
            root.blockIndexToFocusOn = blockIndex;
        }

        function onBlockToFocusOnChanged(blockIndex : int) {
            root.blockIndexToFocusOn = blockIndex;
            root.blockToFocusOn(blockIndex);
        }

        function onRestoreCursorPosition (cursorPosition : int) {
            root.lastCursorPos = cursorPosition;
        }

        function onRestoreSelection(blockStartIndex : int, blockEndIndex : int, firstBlockSelectionStart : int, lastBlockSelectionEnd : int) {
            blockStartIndex = (blockStartIndex - 1) >= 0 ? (blockStartIndex - 1) : 0; // We get the start of the removel index so we need to decrease it by 1
            root.selectedBlockIndexes = [];
            for (var i = blockStartIndex; i <= blockEndIndex; i++) {
                root.selectedBlockIndexes.push(i);
            }
            selectionArea.selStartIndex = blockStartIndex;
            selectionArea.selEndIndex = blockEndIndex;
            selectionArea.selStartPos = firstBlockSelectionStart;
            selectionArea.selEndPos = lastBlockSelectionEnd;
            selectionArea.selectionChanged();
            console.log("blockStartIndex:", blockStartIndex);
            console.log("blockEndIndex:", blockEndIndex);
            console.log("firstBlockSelectionStart:", firstBlockSelectionStart);
            console.log("lastBlockSelectionEnd:", lastBlockSelectionEnd);
        }


    }

    function blockToFocusOn(blockIndex : int) {
        console.log("In blockToFocusOn 0");
        console.log("blockIndex: ", blockIndex);
        var block = blockEditorView.itemAtIndex(blockIndex);
        if (block !== null && block.textEditorPointer !== null) {
            console.log("In blockToFocusOn");
            console.log("blockIndex: ", blockIndex);
            block.textEditorPointer.cursorVisible = true;
            block.textEditorPointer.cursorShowed();
            block.textEditorPointer.cursorPosition = root.lastCursorPos;
            block.textEditorPointer.forceActiveFocus();
            root.selectedBlockIndexes = [blockIndex];
            root.selectedBlock = blockEditorView.itemAtIndex(blockIndex);
            console.log("selectedBlock 2: ", blockIndex);
            console.log("root.selectedBlockIndexes: 3", root.selectedBlockIndexes);
            selectionArea.selStartIndex = blockIndex;
            selectionArea.selStartPos = block.textEditorPointer.cursorPosition;
            [selectionArea.selEndIndex, selectionArea.selEndPos] = [selectionArea.selStartIndex, selectionArea.selStartPos];
            selectionArea.selectionChanged();
        }
    }

    function positionViewAtTopAndSelectFirstBlock () {
        verticalScrollBar.keepActive();
        if (!blockEditorView.atYBeginning) {
            blockEditorView.positionViewAtBeginning();
            blockEditorView.contentY -= blockEditorView.topMargin;
        }
        root.lastCursorPos = 0;
        console.log("lastCursorPos CHANGED 1:", root.lastCursorPos);
        root.blockToFocusOn(0);
    }

    onLastCursorPosChanged: {
//        console.log("lastCursorPos ROOT: ", root.lastCursorPos);
    }

    function positionViewAtBottomAndSelectLastBlock () {
        verticalScrollBar.keepActive();
        if (!blockEditorView.atYEnd) {
            blockEditorView.positionViewAtEnd();
            blockEditorView.contentY += blockEditorView.bottomMargin*2;
        }
        root.lastCursorPos = blockEditorView.itemAtIndex(blockEditorView.count - 1).textEditorPointer.length;
        console.log("lastCursorPos CHANGED 2:", root.lastCursorPos);
        root.blockToFocusOn(blockEditorView.count - 1);
    }

    function pageDown () {
        if (!blockEditorView.atYEnd) {
            verticalScrollBar.keepActive();
            blockEditorView.contentY = Math.min(blockEditorView.contentY + blockEditorView.height, blockEditorView.contentHeight - blockEditorView.height);;
            let middleBlock = blockEditorView.itemAt(blockEditorView.width/2, blockEditorView.contentY + blockEditorView.height/2);
            if (middleBlock) {
                root.lastCursorPos = middleBlock.textEditorPointer.length/2;
                console.log("lastCursorPos CHANGED 3:", root.lastCursorPos);
                blockToFocusOn(middleBlock.index);
            }
        }
    }

    function pageUp () {
        if (!blockEditorView.atYBeginning) {
            verticalScrollBar.keepActive();
            blockEditorView.contentY = Math.max(0, blockEditorView.contentY - blockEditorView.height);
            let middleBlock = blockEditorView.itemAt(blockEditorView.width/2, blockEditorView.contentY + blockEditorView.height/2);
            if (middleBlock) {
                root.lastCursorPos = middleBlock.textEditorPointer.length/2;
                console.log("lastCursorPos CHANGED 4:", root.lastCursorPos);
                blockToFocusOn(middleBlock.index);
            }
        }
    }

    function selectAll () {
        root.selectedBlockIndexes = [];
        var indexes = Array.from({ length: BlockModel.rowCount() }, (_, i) => i);
        root.selectedBlockIndexes = root.selectedBlockIndexes.concat(indexes);
        selectionArea.selStartIndex = 0;
        selectionArea.selEndIndex = BlockModel.rowCount() - 1;
        selectionArea.selStartPos = 0;
        selectionArea.selEndPos = BlockModel.getBlockTextLengthWithoutIndentAndDelimiter(BlockModel.rowCount() - 1); //blockEditorView.itemAtIndex(blockEditorView.count - 1).textEditorPointer.length;
        selectionArea.selectionChanged();
    }

//    onSelectedBlockChanged: {
//        if (root.selectedBlock)
//            console.log("SELECTED BLOCK CHANGED: ", root.selectedBlock.index, root.selectedBlock.blockTextPlainText);
//    }

//    Rectangle {
//        id: animatedCursor
//        property bool showCursor: true
//        visible: showCursor && root.selectedBlock && root.selectedBlockIndexes.length <= 1 && root.selectedBlock.textEditorPointer.selectedText.length === 0
//        width: root.selectedBlock ? root.selectedBlock.blockType === BlockInfo.Heading ? 3 : 2 : 0
//        color: "#007bff"
//        z: 1
//        // TODO: Is there a better way to make these bindings work than these weird conditions?
//        //        property rect selectedBlockCursorRect: root.canUpdateCursorPos && root.selectedBlock && root.selectedBlock.textEditorPointer.height > 0 ? root.selectedBlock.textEditorPointer.positionToRectangle(root.selectedBlock.textEditorPointer.cursorPosition) : root.selectedBlock? root.lastCursorRect : Qt.rect(0,0,0,0)
//        property rect selectedBlockCursorRect: root.canUpdateCursorPos && root.selectedBlock ? root.selectedBlock.textEditorPointer.cursorRectangle : root.selectedBlock ? root.lastCursorRect : Qt.rect(0,0,0,0)
//        height: root.selectedBlock ? root.selectedBlock.textEditorPointer.cursorRectangle.height : 0 //selectedBlockCursorRect.height : 0
////        x: root.selectedBlock && root.selectedBlock.textEditorPointer.x > -200 && root.selectedBlock.delimiterAndTextRowPointer.x >= -200 ? root.mapFromItem(root.selectedBlock.textEditorPointer, selectedBlockCursorRect.x, selectedBlockCursorRect.y).x : 0
////        y: root.selectedBlock && root.selectedBlock.y >= 0 && root.selectedBlock.textEditorPointer.y > -200 && blockEditorView.contentY > -200 ? root.mapFromItem(root.selectedBlock.textEditorPointer, selectedBlockCursorRect.x, selectedBlockCursorRect.y).y : 0
////        x: root.mapFromItem(root.selectedBlock.textEditorPointer, selectedBlockCursorRect.x, selectedBlockCursorRect.y).x
////        y: root.mapFromItem(root.selectedBlock.textEditorPointer, selectedBlockCursorRect.x, selectedBlockCursorRect.y).y
////        x: root.mapFromItem(root.selectedBlock.cursorDelegatePointer, root.selectedBlock.cursorDelegatePointer.x, root.selectedBlock.cursorDelegatePointer.y).x
////        y: root.mapFromItem(root.selectedBlock.cursorDelegatePointer, root.selectedBlock.cursorDelegatePointer.x, root.selectedBlock.cursorDelegatePointer.y).y
//        property bool enabledPressAnimation: false

//        Behavior on x {
//            enabled: root.enableCursorAnimation
//            SmoothedAnimation {
//                duration: root.cursorCurrentAnimationSpeed
//                easing.type: Easing.OutExpo
//            }
//        }

//        Behavior on y {
//            enabled: root.enableCursorAnimation && (!verticalScrollBar.active || root.isAnyKeyPressed)
//            SmoothedAnimation {
//                duration: root.cursorCurrentAnimationSpeed
//                easing.type: Easing.OutExpo
//            }
//        }

//        Behavior on height {
//            SmoothedAnimation {
//                duration: root.cursorCurrentAnimationSpeed
//                easing.type: Easing.OutExpo
//            }
//        }

//        Connections {
//            target: root

//            function onAnyKeyPressed () {
//                animatedCursor.showCursor = true;
//            }

//            function onCursorHidden () {
//                animatedCursor.showCursor = false;
//            }

//            function onCursorShowed () {
//                animatedCursor.showCursor = true;
//            }
//        }

//        // TODO: this might take some uneccesary CPU compute on idle
//        // We need to disable this when the window isn't in focus
//        SequentialAnimation {
//            loops: Animation.Infinite
//            running: root.cursorAnimationRunning

//            PropertyAction {
//                target: animatedCursor
//                property: "showCursor"
//                value: true
//            }

//            PauseAnimation {
//                duration: 500
//            }

//            PropertyAction {
//                target: animatedCursor
//                property: "showCursor"
//                value: false
//            }

//            PauseAnimation {
//                duration: 500
//            }
//        }
//    }

    ListView {
        id: blockEditorView
        clip: true
        model: BlockModel
        topMargin: 45
        bottomMargin: 45
        anchors.fill: parent
        reuseItems: true // Gives huge performance boost

        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Down || event.key === Qt.Key_Up ||
                event.key === Qt.Key_Right || event.key === Qt.Key_Left) {
                if (root.selectedBlockIndexes.length > 0){
                    if(!blockEditorView.itemAtIndex(root.selectedBlockIndexes[0])) {
                        // If there's a selected item but it's out-of-view, we bring him into view
                        console.log("Pressed while out-of-view: ", event.key);
                        blockEditorView.positionViewAtIndex(root.selectedBlockIndexes[0], ListView.Center);
                        verticalScrollBar.keepActive();
                        event.accepted = true;
                    }
                }
            }
        }

        Keys.onReleased: (event) => {
             root.cursorAnimationRunning = true;
             root.isAnyKeyPressed = false;
             root.enableCursorAnimation = true;
             root.cursorCurrentAnimationSpeed = root.cursorDefaultAnimationSpeed;

            if (event.key === Qt.Key_Control) {
                event.accepted = true;
                root.isHoldingControl = false;
            }
        }

        delegate: Rectangle {
            id: delegate
            required property int index
            required property string blockTextPlainText
            required property string blockTextHtml
            required property int blockType
            required property int blockLineStartPos
            required property int blockLineEndPos
            required property int blockIndentLevel
            required property string blockDelimiter
            required property var blockChildren
            required property var blockMetaData

            property alias textEditorPointer: textEditor
            property alias delimiterAndTextRowPointer: delimiterAndTextRow
//            property var cursorDelegatePointer//: cursorDelegateObject //textEditor.cursorDelegate
            property int lastBlockType: {lastBlockType = blockType}
            property int spaceBetweenDelimiterAndText: if(delegate.blockType === BlockInfo.Quote ||
                                                       delegate.blockType === BlockInfo.BulletListItem ||
                                                       delegate.blockType === BlockInfo.NumberedListItem ||
                                                       delegate.blockType === BlockInfo.Todo) {
                                                           20
                                                        } else if (delegate.blockType === BlockInfo.DropCap) {
                                                           10
                                                        } else if (delegate.blockType === BlockInfo.Divider) {
                                                           1
                                                        } else {
                                                           0
                                                       }

            property int spaceBeforeDelimiter: spaceBetweenDelimiterAndText > 0 && delegate.blockType !== BlockInfo.DropCap && delegate.blockType !== BlockInfo.Divider ? 10 : 0
            property var delimiterObject: if (delegate.blockType === BlockInfo.Quote) {
                                                quoteDelimiter
                                            } else if (delegate.blockType === BlockInfo.BulletListItem) {
                                                bulletDelimiter
                                            } else if (delegate.blockType === BlockInfo.Todo) {
                                                todoDelimiter
                                            } else if (delegate.blockType === BlockInfo.NumberedListItem) {
                                                numberedListDelimiter
                                            } else if (delegate.blockType === BlockInfo.DropCap) {
                                                dropCapDelimiter
                                            } else if (delegate.blockType === BlockInfo.Divider) {
                                              dividerDelimiter
                                            } else {
                                                null
                                            }
            property int headingLevel: delegate.blockType === BlockInfo.Heading ? delegate.blockDelimiter.split("#").length - 1 : -1
            property int previousBlockType: if (delegate.index > 0) {
                                                BlockModel.getBlockType(delegate.index - 1) // TODO: Is this gonna slow things down?
                                            } else {
                                                -1
                                            }
            property int nextBlockType: if (delegate.index > 0 && delegate.index + 1 < BlockModel.rowCount()) {
                                                BlockModel.getBlockType(delegate.index + 1) // TODO: Is this gonna slow things down?
                                            } else {
                                                -1
                                            }
            property bool isPooled: false

            width: root.width
            height: textEditor.implicitHeight
            color: root.themeData.backgroundColor
            border.width: root.showBlockBorder ? 1 : 0
            border.color: "red"

            onBlockDelimiterChanged: {
                if (!delegate.isPooled && delegate.lastBlockType === BlockInfo.RegularText && blockType !== BlockInfo.RegularText) {
                    root.lastCursorPos -= blockDelimiter.length;
                    console.log("lastCursorPos CHANGED 20:", root.lastCursorPos);
                    textEditor.cursorPosition -= blockDelimiter.length;
                    delegate.lastBlockType = delegate.blockType;
                }
            }

//            onBlockTypeChanged: {
                // If we want animation on delimiter creation
//                if (root.isProgrammaticChange) {
//                    if (delegate.blockType === BlockInfo.Quote) {
//                        delegate.delimiterObject = quoteDelimiter;
//                        delimiterCreationAnimation.start();
//                    } else if (delegate.blockType === BlockInfo.BulletListItem) {
//                        delegate.delimiterObject = bulletDelimiter;
//                        delimiterCreationAnimation.start();
//                    } else if (delegate.blockType === BlockInfo.Todo) {
//                        delegate.delimiterObject = todoDelimiter;
//                        delimiterCreationAnimation.start();
//                    }
//                }
//            }

//            PropertyAnimation {
//                id: delimiterCreationAnimation
//                target: delegate.delimiterObject
//                property: "opacity"
//                from: 0
//                to: 1
//                duration: 400
//                easing.type: Easing.OutExpo
//            }

            PropertyAnimation {
                id: blockCreationDelegateAnimation
                target: delegate
                property: "height"
                from: 0
                to: textEditor.implicitHeight
                duration: 300
                easing.type: Easing.OutExpo

                onFinished: {
                    delegate.height = Qt.binding(function () { return textEditor.implicitHeight });
                }
            }

            PropertyAnimation {
                id: blockCreationTextAnimation
                target: textEditor
                property: "height"
                from: 0
                to: textEditor.implicitHeight
                duration: 300
                easing.type: Easing.OutExpo

                onFinished: {
                    textEditor.height = Qt.binding(function () { return textEditor.implicitHeight });
                }
            }

            PropertyAnimation {
                id: blockDeletionDelegateAnimation
                target: delegate
                property: "height"
                to: 0
                duration: 300
                easing.type: Easing.OutExpo

//                onFinished: {
//                    BlockModel.backSpacePressedAtStartOfBlock(delegate.index);
//                }
            }

            PropertyAnimation {
                id: blockDeletionTextAnimation
                target: textEditor
                property: "height"
                to: 0
                duration: 300
                easing.type: Easing.OutExpo
            }

            Behavior on height {
                enabled: !delegate.isPooled
                SmoothedAnimation {
                    duration: 300
                    easing.type: Easing.OutExpo
                }
            }

            ListView.onPooled: {
                delegate.isPooled = true;
//                if (root.selectedBlock === delegate)
                textEditor.cursorAnimationRunning = false;
                textEditor.cursorHidden();

//                console.log("pooled: ", delegate.blockTextPlainText);
            }

            ListView.onReused: {
                delegate.isPooled = false;
                textEditor.updateSelection();
                if (root.selectedBlockIndexes.length === 1 && root.selectedBlockIndexes[0] === delegate.index) {
                    console.log("on Completed 2");
                    console.log(root.lastCursorPos);
                    textEditor.cursorAnimationRunning = true;
                    textEditor.cursorShowed();
                    textEditor.cursorPosition = root.lastCursorPos;
                    textEditor.forceActiveFocus();
                }
//                console.log("reused: ", delegate.blockTextPlainText);
            }

            ListView.onAdd: {
                if (root.blockIndexToFocusOn !== -1 && delegate.index === root.blockIndexToFocusOn) {
                    root.selectedBlockIndexes = [delegate.index];
                    blockCreationDelegateAnimation.start();
                    blockCreationTextAnimation.start();
//                    textEditorPointer.cursorPosition = 0;
                    textEditorPointer.cursorPosition = root.lastCursorPos;
                    textEditorPointer.forceActiveFocus();
                    root.selectedBlock = delegate;
                    console.log("selectedBlock 3: ", delegate.index);
                    root.blockIndexToFocusOn = -1;
                    root.enableCursorAnimation = true;
                    root.canUpdateCursorPos = true;
                }
            }

            Component.onCompleted: {
                if (root.blockIndexToFocusOn !== -1 && delegate.index === root.blockIndexToFocusOn) {
                    root.selectedBlockIndexes = [delegate.index];
                    selectionArea.selStartIndex = delegate.index;
                    selectionArea.selStartPos = 0;
                    [selectionArea.selEndIndex, selectionArea.selEndPos] = [selectionArea.selStartIndex, selectionArea.selStartPos]
                    selectionArea.selectionChanged();
//                    root.blockIndexToFocusOn = -1;
                }

                if (root.selectedBlockIndexes.length === 1 && root.selectedBlockIndexes[0] === delegate.index) {
                    textEditorPointer.forceActiveFocus();
                }
            }

            // If we want a vertical line on regular indented text
//            Rectangle {
//                id: indentedRegularTextDelimiter
//                visible: delegate.blockType === BlockInfo.RegularText && delegate.blockIndentLevel > 0 && delegate.previousBlockType === BlockInfo.RegularText
//                x: editorRightLeftPadding + (delegate.blockIndentLevel-1) * root.defaultIndentWidth
//                height: textEditor.height
//                width: 1
//                color: "gray"

//                Behavior on height {
//                    SmoothedAnimation {
//                        duration: 300
//                        easing.type: Easing.OutExpo
//                    }
//                }

//                Behavior on x {
//                    SmoothedAnimation {
//                        duration: 300
//                        easing.type: Easing.OutExpo
//                    }
//                }
//            }

            Row {
                id: delimiterAndTextRow
                property real delimiterAndTextRowX: editorRightLeftPadding + delegate.blockIndentLevel * root.defaultIndentWidth - (delegate.blockType === BlockInfo.Todo ? todoDelimiter.width/2 + 2 : 0)

                Behavior on x {
                    enabled: !delegate.isPooled
                    SmoothedAnimation {
                        duration: 300
                        easing.type: Easing.OutExpo
                    }
                }

                function mirror(value) {
                    return LayoutMirroring.enabled ? (parent.width - width - value) : value;
                }

                x: delimiterAndTextRowX //editorRightLeftPadding + delegate.blockIndentLevel * root.defaultIndentWidth - (delegate.blockType === BlockInfo.Todo ? todoDelimiter.width/2 + 2 : 0)

                Item {
                    visible: delegate.spaceBeforeDelimiter > 0
                    width: delegate.spaceBeforeDelimiter
                    height: 1
                }

                Rectangle {
                    id: quoteDelimiter
                    visible: delegate.blockType === BlockInfo.Quote
                    height: textEditor.height
                    width: 4
                    radius: width
                    color: root.accentColor

                    Behavior on height {
                        enabled: !delegate.isPooled
                        SmoothedAnimation {
                            duration: 300
                            easing.type: Easing.OutExpo
                        }
                    }
                }

                Rectangle {
                    id: dividerBackground
                    visible: delegate.blockType === BlockInfo.Divider
                    color: root.selectedBlockIndexes.length > 1 && root.selectedBlockIndexes.includes(delegate.index) ? root.selectionColor : root.themeData.backgroundColor
                    height: delegate.height //- 2
//                    y: 1
                    width: root.editorWidth - delimiterAndTextRow.x - editorRightLeftPadding

                    Rectangle {
                        id: dividerDelimiter
                        visible: delegate.blockType === BlockInfo.Divider
                        anchors.verticalCenter: parent.verticalCenter
                        height: 1
                        width: root.editorWidth - delimiterAndTextRow.x - editorRightLeftPadding
                        color: root.themeData.theme === "Dark" ? "#525354" : "#d9d9d9"
                    }
                }

                Text {
                    id: numberedListDelimiter
                    visible: delegate.blockType === BlockInfo.NumberedListItem
                    anchors.top: textEditor.top
                    anchors.topMargin: 2
                    text: delegate.blockDelimiter
                    color: root.currentEditorTextColor
                    font.family: root.currentlySelectedFontFamily
                    font.pointSize: root.currentFontPointSize
                }

                Text {
                    id: bulletDelimiter
                    visible: delegate.blockType === BlockInfo.BulletListItem
                    anchors.verticalCenter: textEditor.lineCount <= 1 ? textEditor.verticalCenter : undefined // Why this doesn't update after change?
                    anchors.verticalCenterOffset: -2
                    anchors.top: textEditor.lineCount > 1 ? textEditor.top : undefined
                    anchors.topMargin: 9
                    text: fontIconLoader.icons.fa_circle
                    font.family: fontIconLoader.fontAwesomeSolid.name
                    color: root.accentColor
                    font.pointSize: 6 + root.pointSizeOffset
                }

                FontMetrics {
                    id: dropCapfontMetrics
                    font.family: root.currentlySelectedFontFamily
                    font.pixelSize: dropCapDelimiter.dropCapPixelSize
                }

                Text {
                    // TODO: this is not ideal and only work with some fonts. We need to find a proper way to calculate
                    // the height of different fonts
                    id: dropCapDelimiter
                    property real dropCapPixelSize: textEditor.height + 0.50*textEditor.font.pixelSize*(textEditor.lineCount-1)
                    padding: 0
                    y: -dropCapfontMetrics.xHeight/2 - (textEditor.lineCount - 1)/2
                    height: implicitHeight - dropCapfontMetrics.descent
                    visible: delegate.blockType === BlockInfo.DropCap
                    text: delegate.blockDelimiter.length > 1 ? delegate.blockDelimiter[1] : ""
                    color: root.currentEditorTextColor // root.headingColor?
                    font.family: root.currentlySelectedFontFamily
                    font.pixelSize: dropCapPixelSize > 300 ? 300 : dropCapPixelSize
                    font.weight: Font.Bold
                    wrapMode: Text.NoWrap

                    Behavior on font.pixelSize {
                        enabled: !delegate.isPooled
                        SmoothedAnimation {
                            duration: 300
                            easing.type: Easing.OutExpo
                        }
                    }
                }

                CheckBoxMaterial {
                    id: todoDelimiter
                    visible: delegate.blockType === BlockInfo.Todo
                    anchors.top: textEditor.top
                    anchors.topMargin: (-height/4)
                    theme: root.themeData.theme
                    checked: delegate.blockType === BlockInfo.Todo && delegate.blockMetaData["taskChecked"] ? true : false

                    onTaskChecked: {
                        root.selectedBlockIndexes = [delegate.index];
                        root.selectedBlock = delegate;
                        console.log("root.selectedBlockIndexes: 18", root.selectedBlockIndexes);
                        BlockModel.toggleTaskAtIndex(delegate.index);
                    }

                    onTaskUnchecked: {
                        root.selectedBlockIndexes = [delegate.index];
                        root.selectedBlock = delegate;
                        console.log("root.selectedBlockIndexes: 19", root.selectedBlockIndexes);
                        BlockModel.toggleTaskAtIndex(delegate.index);
                    }
                }

                Item {
                    id: spacerBetweenDelimiterAndText
                    visible: delegate.spaceBetweenDelimiterAndText > 0 || (delegate.blockType === BlockInfo.RegularText && delegate.blockIndentLevel > 0)
                    width: ((delegate.blockType === BlockInfo.Todo || delegate.blockType === BlockInfo.NumberedListItem) ? delegate.spaceBetweenDelimiterAndText/2 : delegate.spaceBetweenDelimiterAndText) + (delegate.blockType === BlockInfo.Todo ? 2 : 0)
                    height: 1
                }

                TextArea {
//                    renderType: TextArea.NativeRendering
                    function getHeaderPointSizeMultiplier(headerSize) {
                        if (headerSize < 1 || headerSize > 6)
                            return 6;

                        const mapping = {
                            1: 1.9,
                            2: 1.72,
                            3: 1.54,
                            4: 1.36,
                            5: 1.18,
                            6: 1
                        };

                        return mapping[headerSize];
                    }

                    id: textEditor

                    leftPadding: 0
                    topPadding: if (delegate.blockType === BlockInfo.Heading && delegate.index > 0) {
                                    8
                                } else if ((textEditor.length === 0 && delegate.previousBlockType === BlockInfo.Heading) ||
                                           (delegate.blockType === BlockInfo.Todo && delegate.previousBlockType === BlockInfo.Todo) ||
                                           (delegate.blockType === BlockInfo.BulletListItem && delegate.previousBlockType === BlockInfo.BulletListItem) ||
                                           (delegate.blockType === BlockInfo.NumberedListItem && delegate.previousBlockType === BlockInfo.NumberedListItem)) {
                                    0
                                } else {
                                    1.8
                                }
                    bottomPadding: if (delegate.blockType === BlockInfo.Heading) {
                                       8
                                   } else if ((textEditor.length === 0 && delegate.previousBlockType === BlockInfo.Heading) ||
                                              (delegate.blockType === BlockInfo.Todo && delegate.nextBlockType === BlockInfo.Todo) ||
                                              (delegate.blockType === BlockInfo.BulletListItem && delegate.nextBlockType === BlockInfo.BulletListItem) ||
                                              (delegate.blockType === BlockInfo.NumberedListItem && delegate.nextBlockType === BlockInfo.NumberedListItem)) {
                                       0
                                   } else {
                                       1.8
                                   }
                    padding: 0
                    width: root.editorWidth - delimiterAndTextRow.x - x - editorRightLeftPadding
                    height: implicitHeight
                    wrapMode: TextArea.WordWrap
                    readOnly: delegate.blockType === BlockInfo.Divider
                    text: delegate.blockTextHtml
                    textFormat: TextArea.RichText
                    tabStopDistance: 20
                    // TODO: Qt has a bug where sometimes the strikeout isn't consistently seen, very weird. File bug report.
                    // So we disable this for now...
//                    font.strikeout: delegate.blockType === BlockInfo.Todo && delegate.blockMetaData["taskChecked"] ? true : false
                    font.family: root.currentlySelectedFontFamily
                    font.pointSize: if (delegate.blockType === BlockInfo.Heading) {
                                        root.currentFontPointSize * getHeaderPointSizeMultiplier(delegate.headingLevel)
                                    } else {
                                        root.currentFontPointSize
                                    }
                    font.weight: delegate.blockType === BlockInfo.Heading ? Font.Bold : Font.Normal
                    color: if (delegate.blockType === BlockInfo.Heading) {
                               root.headingColor
                            } else if (delegate.blockType === BlockInfo.Todo && delegate.blockMetaData["taskChecked"]) {
                                   root.todoItemCheckedTextColor
                            } else {
                               root.currentEditorTextColor
                           }
                    selectionColor: root.selectionColor
                    selectedTextColor: color
                    activeFocusOnTab: false
                    background: Rectangle {
                        color: textEditor.selectedText.length === textEditor.length && root.selectedBlockIndexes.length > 1 && root.selectedBlockIndexes.includes(delegate.index) ? root.selectionColor : root.themeData.backgroundColor
                        border.color: "red"
                        border.width: root.showBlockBorder ? 1 : 0
                    }
                    placeholderTextColor: root.themeData.theme === "Dark" ? "#5a5a5a" : "#9b9a97"
                    placeholderText: if (activeFocus) {
                                         if (delegate.blockType === BlockInfo.RegularText) {
                                             "" // qsTr("Enter text or type '/' for commands...")
                                         } else if (delegate.blockType === BlockInfo.BulletListItem ||
                                                    delegate.blockType === BlockInfo.NumberedListItem) {
                                             qsTr("List")
                                         } else if (delegate.blockType === BlockInfo.Todo) {
                                             qsTr("To do")
                                         } else if (delegate.blockType === BlockInfo.Heading) {
                                             qsTr("Heading")
                                         } else  if (delegate.blockType === BlockInfo.Quote) {
                                             qsTr("Quote")
                                         } else {
                                             ""
                                         }
                                     } else {
                                         ""
                                     }
                        //activeFocus ? qsTr("Press '/' for commands...") : ""
                    property bool cursorAnimationRunning: true
                    signal anyKeyPressed
                    signal cursorHidden
                    signal cursorShowed
                    cursorDelegate: Rectangle {
                        id: cursorDelegateObject
                        visible: !delegate.isPooled && root.selectedBlockIndexes.length <= 1 && textEditor.selectedText.length === 0
                        color: root.accentColor //Qt.rgba(Math.random(), Math.random(), Math.random(), 1) //"transparent" //root.accentColor
                        width: delegate.blockType === BlockInfo.Heading ? 3 : 2

//                        Component.onCompleted: {
//                            delegate.cursorDelegatePointer = cursorDelegateObject;
//                        }

                        Connections {
                            target: textEditor

                            function onAnyKeyPressed () {
                                cursorDelegateObject.visible = Qt.binding(function () { return !delegate.isPooled && root.selectedBlockIndexes.length <= 1 && textEditor.selectedText.length === 0 }); // true;
                            }

                            function onCursorHidden () {
                                cursorDelegateObject.visible = false;
                            }

                            function onCursorShowed () {
                                cursorDelegateObject.visible = Qt.binding(function () { return !delegate.isPooled && root.selectedBlockIndexes.length <= 1 && textEditor.selectedText.length === 0 }); // true;
                            }
                        }

                        SequentialAnimation {
                            loops: Animation.Infinite
                            running: !delegate.isPooled && textEditor.cursorAnimationRunning && root.selectedBlockIndexes.length <= 1 && textEditor.selectedText.length === 0

                            PropertyAction {
                                target: cursorDelegateObject
                                property: 'visible'
                                value: true
                            }

                            PauseAnimation {
                                duration: 500
                            }

                            PropertyAction {
                                target: cursorDelegateObject
                                property: 'visible'
                                value: false
                            }

                            PauseAnimation {
                                duration: 500
                            }
                        }
                    }

                    Behavior on x {
                        enabled: !delegate.isPooled
                        SmoothedAnimation {
                            duration: 300
                            easing.type: Easing.OutExpo
                        }
                    }

                    Behavior on height {
                        enabled: !delegate.isPooled
                        SmoothedAnimation {
                            duration: 300
                            easing.type: Easing.OutExpo
                        }
                    }

                    Behavior on font.pointSize {
                        enabled: !delegate.isPooled
                        SmoothedAnimation {
                            duration: 300
                            easing.type: Easing.OutExpo
                        }
                    }

                    onCursorPositionChanged: {
                        root.cursorX = root.mapFromItem(textEditor, textEditor.positionToRectangle(cursorPosition)).x;

                        if (!delegate.isPooled && !root.isProgrammaticChange) {
                            root.lastCursorPos = cursorPosition;
//                            console.log("lastCursorPos CHANGED 5:", root.lastCursorPos);
                        }
                    }

                    onCursorShowed: {
                        root.cursorX = textEditor.positionToRectangle(cursorPosition).x;
                    }

                    onPressed: {
                        cursorShowed();
                        textEditor.cursorAnimationRunning = true;
                        root.cursorAnimationRunning = true;
                    }

                    onLinkHovered: (link) => {
                        root.currentHoveredLink = link;
                        if (link !== "") {
                           selectionArea.cursorShape = Qt.PointingHandCursor;
                        } else {
                           selectionArea.cursorShape = Qt.binding(function () { return selectionArea.enabled ? Qt.IBeamCursor : Qt.ArrowCursor });
                        }
                    }

//                    onCursorVisibleChanged: {
//                        console.log("cursor visible changed:", cursorVisible);
//                    }

                    onFocusChanged: {
//                        console.log("focus changed:", focus);
                        if (!focus) {
                            textEditor.cursorAnimationRunning = false;
                            root.cursorAnimationRunning = false;
                            root.cursorHidden();
                            textEditor.cursorHidden();
                        } else {
                            textEditor.cursorAnimationRunning = true;
                            root.cursorAnimationRunning = true;
                            root.cursorShowed();
                            textEditor.cursorShowed();
                        }
                    }

                    function editingMultipleBlocks(eventKey) {
                        var savedPressedChar = eventKey !== Qt.Key_Backspace ? eventKey : -1;
                        var firstBlockSelectionStart;
                        var lastBlockSelectionEnd;
                        var firstBlockIndex = Math.min(...root.selectedBlockIndexes);
                        if (selectionArea.selStartIndex > selectionArea.selEndIndex) {
                            root.lastCursorPos = selectionArea.selEndPos;
                            console.log("lastCursorPos CHANGED 6:", root.lastCursorPos);
                            firstBlockSelectionStart = selectionArea.selEndPos;
                            lastBlockSelectionEnd = selectionArea.selStartPos;
                        } else {
                            root.lastCursorPos = selectionArea.selStartPos;
                            console.log("lastCursorPos CHANGED 7:", root.lastCursorPos);
                            firstBlockSelectionStart = selectionArea.selStartPos;
                            lastBlockSelectionEnd = selectionArea.selEndPos;
                        }
                        if (eventKey !== Qt.Key_Backspace) root.lastCursorPos += 1;
                        console.log("lastCursorPos CHANGED 21:", root.lastCursorPos);
                        var isPressedCharLower = !root.isHoldingShift && !root.isHoldingCapsLock;
                        BlockModel.editBlocks(root.selectedBlockIndexes, firstBlockSelectionStart, lastBlockSelectionEnd, savedPressedChar, isPressedCharLower);
                        console.log("root.selectedBlockIndexes: 20", root.selectedBlockIndexes);
                        root.blockToFocusOn(firstBlockIndex);
                    }

                    Keys.onPressed: (event) => {
                        anyKeyPressed();
                        textEditor.cursorAnimationRunning = false;
                        root.cursorAnimationRunning = false;
                        root.isAnyKeyPressed = true;

                        if (event.key === Qt.Key_Tab) {
                            event.accepted = true;
                            root.enableCursorAnimation = false;
                            BlockModel.indentBlocks(root.selectedBlockIndexes);
                            return;
                        }

                        if(event.key === Qt.Key_Backtab) {
                            event.accepted = true;
                            root.enableCursorAnimation = false;
                            BlockModel.unindentBlocks(root.selectedBlockIndexes);
                            return;
                        }

                        if (event.key === Qt.Key_Escape) {
                            event.accepted = true;
                            root.showBlockBorder = !root.showBlockBorder;
                            return;
                        }

                        if (event.key === Qt.Key_Shift) {
                                            console.log("SHIFT IS ON");
                            event.accepted = true;
                            root.isHoldingShift = true;
                            return;
                        }

                        if (event.key === Qt.Key_CapsLock) {
                            event.accepted = true;
                            root.isHoldingCapsLock = true;
                            return;
                        }

                        if (event.key === Qt.Key_Control) {
                            event.accepted = true;
                            root.isHoldingControl = true;
                            return;
                        }

                        if (root.isHoldingControl && event.key === Qt.Key_A) {
                            event.accepted = true;
                            root.selectAll();
                            return;
                        }

                        if (root.isHoldingControl && event.key === Qt.Key_C) {
                            event.accepted = true;
                            root.copy();
                            return;
                        }

                        if ((root.isHoldingShift && root.isHoldingControl && event.key === Qt.Key_Z) || (root.isHoldingControl && event.key === Qt.Key_Y)) { // TODO: Why Qt.Key_Redo doesn't work?
                           console.log("REDO QML");
                           event.accepted = true;
                           BlockModel.redo();
                           if (root.selectedBlockIndexes.length > 0)
                                blockEditorView.positionViewAtIndex(root.selectedBlockIndexes[0], ListView.Center);
                           return;
                        } else if (!root.isHoldingShift && root.isHoldingControl && event.key === Qt.Key_Z) { // TODO: Why Qt.Key_Undo doesn't work?
                           console.log("UNDO QML");
                           event.accepted = true;
                           BlockModel.undo();
                           if (root.selectedBlockIndexes.length > 0)
                                blockEditorView.positionViewAtIndex(root.selectedBlockIndexes[0], ListView.Center);
                           return;
                        }

                        if (event.key === Qt.Key_Right) {
                            if (root.selectedBlockIndexes.length === 1 && cursorPosition === textEditor.length && delegate.index + 1 < BlockModel.rowCount()) {
                                root.lastCursorPos = 0;
                                                console.log("lastCursorPos CHANGED 8:", root.lastCursorPos);
                                blockToFocusOn(delegate.index + 1);
                                checkIfToScrollDown();
                            } else if (root.selectedBlockIndexes.length > 1) {
                                                console.log("IN RIGHT");
                                 let actualEndIndex = Math.max(selectionArea.selStartIndex, selectionArea.selEndIndex);
                                 let blockAtEnd = blockEditorView.itemAtIndex(actualEndIndex);
                                 let actualEndPos = selectionArea.selStartIndex < selectionArea.selEndIndex ? selectionArea.selEndPos : selectionArea.selStartPos;
                                 if(blockAtEnd === null || (blockAtEnd && (blockEditorView.contentY > blockAtEnd.y ||
                                             blockEditorView.contentY + blockEditorView.height < root.editorRightLeftPadding + blockAtEnd.y))) {
                                    blockEditorView.positionViewAtIndex(actualEndIndex, ListView.Center);
                                 }
                                 // TODO: Why cursor isn't positioned properly at start?
                                 root.lastCursorPos = actualEndPos;
                                 blockToFocusOn(actualEndIndex);
                            }
                        }

                        if (event.key === Qt.Key_Left) {
                            if (root.selectedBlockIndexes.length === 1 && cursorPosition === 0 && delegate.index > 0) {
                                root.lastCursorPos = 0;
                                let block = blockEditorView.itemAtIndex(delegate.index - 1);
                                root.lastCursorPos = block.textEditorPointer.length;
                                                console.log("lastCursorPos CHANGED 9:", root.lastCursorPos);
                                blockToFocusOn(delegate.index - 1);
                                checkIfToScrollUp();
                            } else if (root.selectedBlockIndexes.length > 1) {
                                                console.log("IN LEFT");
                                 let actualStartIndex = Math.min(selectionArea.selStartIndex, selectionArea.selEndIndex);
                                 let blockAtStart = blockEditorView.itemAtIndex(actualStartIndex);
                                 let actualStartPos = selectionArea.selStartIndex < selectionArea.selEndIndex ? selectionArea.selStartPos : selectionArea.selEndPos;
                                 if (actualStartPos > 0) {
                                     actualStartPos += 1;
                                 }
                                if(blockAtStart === null || (blockAtStart && (blockEditorView.contentY > blockAtStart.y + blockAtStart.height ||
                                                                              blockEditorView.contentY + blockEditorView.contentHeight < blockAtStart.y))) {
                                   blockEditorView.positionViewAtIndex(actualStartIndex, ListView.Center);
                                }
                                 // TODO: Why cursor isn't positioned properly at end
                                 root.lastCursorPos = actualStartPos;
                                 blockToFocusOn(actualStartIndex);
                            }
                        }

                        if (event.key === Qt.Key_Up) {
                            if (root.isHoldingControl) {
                                console.log("Control + UP");
                                event.accepted = true;
                                root.positionViewAtTopAndSelectFirstBlock();
                                return;
                            } else {
                                checkIfToScrollUp();
                                if (delegate.index > 0) {
                                    if (root.selectedBlockIndexes.length > 1) {
                                        let actualStartPos = selectionArea.selStartIndex < selectionArea.selEndIndex ? selectionArea.selStartPos : selectionArea.selEndPos;
                                        cursorPosition = actualStartPos;
                                        root.cursorX = root.mapFromItem(textEditor, textEditor.positionToRectangle(cursorPosition)).x;
                                    }
                                    if (textEditor.positionToRectangle(cursorPosition).y <= textEditor.topPadding) { // TODO: Is this proper way to check if we're on the first line?
                                        if (!root.isCursorMovedVertically) {
                                            root.isCursorMovedVertically = true;
                                            root.cursorXSaved = root.cursorX;
                                        }
                                        let blockAbove = blockEditorView.itemAtIndex(delegate.index - 1);
                                        let cursorXMappedToItemAbove = blockAbove.textEditorPointer.mapFromItem(root, root.cursorXSaved, root.y).x;
                                        let upperDelegatLastLineY = blockAbove.textEditorPointer.positionToRectangle(blockAbove.textEditorPointer.length).y;
                                        root.lastCursorPos = blockAbove.textEditorPointer.positionAt(cursorXMappedToItemAbove, upperDelegatLastLineY);
                                                        console.log("lastCursorPos CHANGED 10:", root.lastCursorPos);
                                        blockToFocusOn(delegate.index - 1);
                                        event.accepted = true;
                                        return;
                                    }
                                } else if (textEditor.positionToRectangle(cursorPosition).y <= textEditor.topPadding){
                                    event.accepted = true;
                                    return;
                                }
                            }
                        }

                        if (event.key === Qt.Key_Down) {
                            if (root.isHoldingControl) {
                                console.log("Control + DOWN");
                                event.accepted = true;
                                root.positionViewAtBottomAndSelectLastBlock();
                                return;
                            } else {
                                checkIfToScrollDown();
                                if (delegate.index + 1 < blockEditorView.count) {
                                    if (root.selectedBlockIndexes.length > 1) {
                                        let actualEndPos = selectionArea.selStartIndex < selectionArea.selEndIndex ? selectionArea.selEndPos : selectionArea.selStartPos;
                                        cursorPosition = actualEndPos;
                                        root.cursorX = root.mapFromItem(textEditor, textEditor.positionToRectangle(cursorPosition)).x;
                                    }
                                    if (textEditor.positionToRectangle(cursorPosition).y + textEditor.positionToRectangle(cursorPosition).height > textEditor.height - textEditor.font.pixelSize / 2) {//(textEditor.topPadding*2+textEditor.font.pixelSize)*textEditor.lineCount) { // TODO: Is this proper way to check if we're on the last line?
                                        if (!root.isCursorMovedVertically) {
                                            root.isCursorMovedVertically = true;
                                            root.cursorXSaved = root.cursorX;
                                        }
                                        let blockBelow = blockEditorView.itemAtIndex(delegate.index + 1);
                                        let cursorXMappedToItemBelow = blockBelow.textEditorPointer.mapFromItem(root, root.cursorXSaved, root.y).x;
                                        let lowerDelegatFirstLineY = blockBelow.textEditorPointer.positionToRectangle(0).y;
                                        root.lastCursorPos = blockBelow.textEditorPointer.positionAt(cursorXMappedToItemBelow, lowerDelegatFirstLineY);
                                                        console.log("lastCursorPos CHANGED 11:", root.lastCursorPos);
                                        blockToFocusOn(delegate.index + 1);
                                        event.accepted = true;
                                        return;
                                    }
                                } else if (textEditor.positionToRectangle(cursorPosition).y + textEditor.positionToRectangle(cursorPosition).height > textEditor.height - textEditor.font.pixelSize / 2) { //> (textEditor.topPadding+textEditor.font.pixelSize)*textEditor.lineCount) {
                                    event.accepted = true;
                                    return;
                                }
                            }
                        }

                        if (event.key === Qt.Key_Right || event.key === Qt.Key_Left) {
                            root.isCursorMovedVertically = false;
                        }

                        if (event.key === Qt.Key_PageDown) {
                            event.accepted = true;
                            root.pageDown();
                            return;
                        }

                        if (event.key === Qt.Key_PageUp) {
                            event.accepted = true;
                            root.pageUp();
                            return;
                        }

                        // TODO: find a better way to detect all these type of keys
                        if (event.key === Qt.Key_CapsLock || event.key === Qt.Key_Control ||
                            event.key === Qt.Key_Meta || event.key === Qt.Key_Option ||
                            event.key === Qt.Key_Control || event.key === Qt.Key_Alt) {
                            event.accepted = true;
                            return;
                        }

                        if (root.selectedBlockIndexes.length > 1) {
                            if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
                                root.selectedBlockIndexes = [index];
                                root.selectedBlock = delegate;
                                                console.log("selectedBlock 6: ", delegate.index);
                                console.log("root.selectedBlockIndexes: 15", root.selectedBlockIndexes);
                                selectionArea.selStartIndex = delegate.index;
                                selectionArea.selStartPos = cursorPosition;
                                [selectionArea.selEndIndex, selectionArea.selEndPos] = [selectionArea.selStartIndex, selectionArea.selStartPos];
                                selectionArea.selectionChanged();
                            } else if (event.key !== Qt.Key_Right && event.key !== Qt.Key_Left) {
                                textEditor.editingMultipleBlocks(event.key);
                                event.accepted = true;
                                return;
                            }
                        }

                        if (event.key === Qt.Key_Backspace) {

                            root.cursorCurrentAnimationSpeed = 30;
                            if (root.selectedBlockIndexes.length === 1 && cursorPosition === 0) {
                                if (delegate.blockIndentLevel === 0 && delegate.blockType === BlockInfo.RegularText && delegate.index > 0) {
                                    var previousBlock = blockEditorView.itemAtIndex(delegate.index - 1);
                                    if (previousBlock !== null && previousBlock.textEditorPointer !== null) {
                                        root.lastCursorPos = previousBlock.textEditorPointer.length;
                                    }
                                }

                                if ((delegate.blockType !== BlockInfo.RegularText && delegate.blockDelimiter.length > 0) || delegate.blockIndentLevel > 0) {
                                    // If not a regular text block or if it is but it is indented
                                    BlockModel.backSpacePressedAtStartOfBlock(delegate.index);
                                    blockToFocusOn(delegate.index);
                                } else if (delegate.index > 0 && !(BlockModel.getBlockType(delegate.index - 1) === BlockInfo.Divider && textEditor.length > 0)) {
                                    // If it's a regular text block and it's not indented, and it's not a divider
                                    BlockModel.moveBlockTextToBlockAbove(delegate.index);
                                    blockToFocusOn(delegate.index - 1);
                                    BlockModel.backSpacePressedAtStartOfBlock(delegate.index);
//                                    blockDeletionDelegateAnimation.start(); // When the animation finishes we'll call backSpacePressedAtStartOfBlock
//                                    blockDeletionTextAnimation.start();
                                }
                            }
                        }
                    }

                    function checkIfToScrollDown() {
                        if(blockEditorView.contentY > delegate.y ||
                           blockEditorView.contentY + blockEditorView.height < root.editorRightLeftPadding + delegate.y) {
                            // If there's a selected item but it's out-of-view, we bring him into view
                            blockEditorView.positionViewAtIndex(delegate.index, ListView.Center);
                            verticalScrollBar.keepActive();
                        }
                        let threshold = 100;
                        let bottomLimit =  blockEditorView.contentY + blockEditorView.height - threshold;
                        if (delegate.y + textEditor.positionToRectangle(cursorPosition).y < blockEditorView.y + blockEditorView.contentHeight && delegate.y + textEditor.positionToRectangle(cursorPosition).y + textEditor.positionToRectangle(cursorPosition).height*2 >= bottomLimit) {
                            blockEditorView.contentY += textEditor.font.pixelSize*2;
                            verticalScrollBar.keepActive();
                        }
                    }

                    function checkIfToScrollUp() {
                        if(blockEditorView.contentY > delegate.y + delegate.height ||
                           blockEditorView.contentY + blockEditorView.contentHeight < delegate.y) {
                            // If there's a selected item but it's out-of-view, we bring him into view
                            blockEditorView.positionViewAtIndex(delegate.index, ListView.Center);
                            verticalScrollBar.keepActive();
                        }
                        let threshold = 100;
                        let topLimit =  threshold;
                        if (blockEditorView.contentY > 0 && delegate.mapToItem(selectionArea, x, y).y < topLimit) {
                            blockEditorView.contentY -= textEditor.font.pixelSize*2;
                            verticalScrollBar.keepActive();
                        }
                    }

                    Keys.onReturnPressed: {
                        anyKeyPressed();
                        textEditor.cursorAnimationRunning = false;
                        root.cursorAnimationRunning = false;
                        root.isAnyKeyPressed = true;

                        if (root.isHoldingControl && delegate.blockType === BlockInfo.Todo) {
                            BlockModel.toggleTaskAtIndex(delegate.index);
                        } else if (!root.isHoldingControl){
                            if (root.selectedBlockIndexes.length === 1) {
                                if (root.isHoldingShift) {
                                    // soft break
                                    let savedCursorPositionForUndo = textEditor.cursorPosition;
                                    root.isProgrammaticChange = true;
                                    textEditor.insert(cursorPosition, "<br />"); // It proved too difficult to do this simple thing in the C++ model due to inconssitencies between the qml and c++ formatting of html/markdown
                                    root.lastCursorPos = textEditor.cursorPosition;
                                    BlockModel.setTextAtIndex(delegate.index, textEditor.getFormattedText(0, textEditor.length), savedCursorPositionForUndo);
                                    checkIfToScrollDown();
                                } else {
                                    // hard break
                                    if (cursorPosition === textEditor.length) {
                                        // cursor is at end of text
                                        if (textEditor.length === 0 && delegate.blockType !== BlockInfo.RegularText && delegate.blockType !== BlockInfo.Divider) {
                                            // if text length == 0 and user hits enter, we usually want to remove the delimiter
                                            BlockModel.backSpacePressedAtStartOfBlock(delegate.index);
                                        } else {
                                            // cursor is at end of text and the text is not empty
                                            if (delegate.blockType === BlockInfo.Quote || delegate.blockType === BlockInfo.DropCap) {
                                                // If a quote block or a drop cap
                                                if (textEditor.getText(cursorPosition - 1, cursorPosition) === "\u2028") { // Unicode line separator
                                                    // There's a line break at the last line without text
                                                    root.lastCursorRect = textEditor.cursorRectangle;
                                                    root.canUpdateCursorPos = false;
                                                    BlockModel.setTextAtIndex(delegate.index, textEditor.getFormattedText(0, textEditor.length - 1), textEditor.cursorPosition);
                                                    console.log("delegate.index 1: ", delegate.index);
                                                    BlockModel.insertNewBlock(delegate.index, "", true);
                                                    checkIfToScrollDown();
                                                } else {
                                                    // There's text at the last line, therefore
                                                    // imitate soft break
                                                    let savedCursorPositionForUndo = textEditor.cursorPosition;
                                                    root.isProgrammaticChange = true;
                                                    textEditor.insert(cursorPosition, "<br />"); // It proved too difficult to do this simple thing in the C++ model due to inconssitencies between the qml and c++ formatting of html/markdown
                                                    root.lastCursorPos = cursorPosition;
                                                    BlockModel.setTextAtIndex(delegate.index, textEditor.getFormattedText(0, textEditor.length), savedCursorPositionForUndo);
                                                    checkIfToScrollDown();
                                                }
                                            } else if (delegate.blockType === BlockInfo.RegularText && textEditor.length === 0 && delegate.blockIndentLevel > 0){
                                                // If not a quote block and cursor is at the end but indented, we unindent
                                                BlockModel.unindentBlocks([delegate.index]);
                                            } else {
                                                // hard break
                                                console.log("delegate.index 2: ", delegate.index);
                                                BlockModel.insertNewBlock(delegate.index, "");
                                                checkIfToScrollDown();
                                            }
                                        }
                                    } else {
                                        if (delegate.blockType === BlockInfo.Quote || delegate.blockType === BlockInfo.DropCap) {
                                            // imitate soft break
                                            let savedCursorPositionForUndo = textEditor.cursorPosition;
                                            root.isProgrammaticChange = true;
                                            textEditor.insert(cursorPosition, "<br />"); // It proved too difficult to do this simple thing in the C++ model due to inconssitencies between the qml and c++ formatting of html/markdown
                                            root.lastCursorPos = cursorPosition;
                                            BlockModel.setTextAtIndex(delegate.index, textEditor.getFormattedText(0, textEditor.length), savedCursorPositionForUndo);
                                            checkIfToScrollDown();
                                        } else {
                                            // hard break with saving text
                                            var savedText = textEditor.getFormattedText(cursorPosition, textEditor.length);
                                            BlockModel.setTextAtIndex(delegate.index, textEditor.getFormattedText(0, cursorPosition), textEditor.cursorPosition);
                                            console.log("delegate.index 3: ", delegate.index);
                                            BlockModel.insertNewBlock(delegate.index, savedText, true);
                                            checkIfToScrollDown();
                                        }
                                    }
                                }
                            } else if (root.selectedBlockIndexes.length > 1) {
                                if (root.isHoldingShift) {
                                    editingMultipleBlocks(-1);
                                    textEditor.insert(cursorPosition, "<br />");
                                    BlockModel.setTextAtIndex(delegate.index, textEditor.getFormattedText(0, textEditor.length), textEditor.cursorPosition);
                                } else {
                                    editingMultipleBlocks(-1);
                                    console.log("delegate.index 4: ", delegate.index);
                                    BlockModel.insertNewBlock(delegate.index, "");
                                    checkIfToScrollDown();
                                }
                            }
                        }
                    }

                    Keys.onReleased: (event) => {
                         textEditor.cursorAnimationRunning = true;
                         root.cursorAnimationRunning = true;
                         root.isAnyKeyPressed = false;
                         root.enableCursorAnimation = true;
                         root.cursorCurrentAnimationSpeed = root.cursorDefaultAnimationSpeed;

                         if (event.key === Qt.Key_Shift) {
                            event.accepted = true;
                            root.isHoldingShift = false;
                         }

                         if (event.key === Qt.Key_CapsLock) {
                            event.accepted = true;
                            root.isHoldingCapsLock = false;
                         }

                        if (event.key === Qt.Key_Control) {
                            event.accepted = true;
                            root.isHoldingControl = false;
                        }
                    }

                    onTextChanged: {
//                        console.log("BLOCK TEXT CHANGED: ", delegate);
                        if (!delegate.isPooled) {

//                            cursorPosition = root.lastCursorPos;
                            if (root.selectedBlock === delegate && root.isAnyKeyPressed && !root.isProgrammaticChange && root.selectedBlockIndexes.length <= 1) {
                                if(delegate.blockType !== BlockInfo.Divider) {
                                    console.log("In onTextChanged QML");
                                    BlockModel.setTextAtIndex(delegate.index, textEditor.getFormattedText(0, textEditor.length), textEditor.cursorPosition);
                                    delegate.lastBlockType = delegate.blockType;
                                    console.log("root.lastCursorPos 12:", root.lastCursorPos);
                                    cursorPosition = root.lastCursorPos;
//                                    root.selectedBlockIndexes = [delegate.index];
//                                    root.selectedBlock = delegate;
//                                    console.log("selectedBlock 7: ", delegate.index);
//                                    console.log("root.selectedBlockIndexes: 12", root.selectedBlockIndexes);
                                }
                            }
                        }
                    }

                    Connections {
                        target: selectionArea
                        function onSelectionChanged() {
//                            console.log("In onSelectionChanged");
                            textEditor.updateSelection();
                        }
                    }

                    Component.onCompleted: {
                        updateSelection()
                    }

                    function appendDelegateIndexToSelectedBlocks() {
                        if (!root.selectedBlockIndexes.includes(delegate.index)) {
                            root.selectedBlockIndexes.push(delegate.index);
                            console.log("root.selectedBlockIndexes: 17", root.selectedBlockIndexes);
                        }
                    }

                    function updateSelection() {
//                        console.log("selectionArea.selStartIndex: ", selectionArea.selStartIndex);
//                        console.log("selectionArea.selEndIndex: ", selectionArea.selEndIndex);

                        var actualStartIndex = Math.min(selectionArea.selStartIndex, selectionArea.selEndIndex);
                        var actualEndIndex = Math.max(selectionArea.selStartIndex, selectionArea.selEndIndex);
                        var actualStartPos = selectionArea.selStartIndex < selectionArea.selEndIndex ? selectionArea.selStartPos : selectionArea.selEndPos;
                        var actualEndPos = selectionArea.selStartIndex < selectionArea.selEndIndex ? selectionArea.selEndPos : selectionArea.selStartPos;

//                        console.log("delegate index:", delegate.index);
//                        console.log("actualStartIndex: ", actualStartIndex);
//                        console.log("actualEndIndex: ", actualEndIndex);

                        if (delegate.index < actualStartIndex || delegate.index > actualEndIndex) {
                            textEditor.deselect();
                            let delegateIndexInSelectedIndexes = root.selectedBlockIndexes.indexOf(delegate.index);
                            if (delegateIndexInSelectedIndexes !== -1) {
//                                console.log("root.selectedBlockIndexes: 7", root.selectedBlockIndexes);
                                root.selectedBlockIndexes.splice(delegateIndexInSelectedIndexes, 1);
//                                console.log("root.selectedBlockIndexes: 7.2", root.selectedBlockIndexes);
//                                console.log("delegate.index: ", delegate.index)
//                                console.log("actualStartIndex: ", actualStartIndex);
//                                console.log("actualEndIndex: ", actualEndIndex);
                            }
                        } else if (delegate.index > actualStartIndex && delegate.index < actualEndIndex) {
                            textEditor.selectAll();
                            appendDelegateIndexToSelectedBlocks();
                        } else if (delegate.index === actualStartIndex && delegate.index === actualEndIndex) {
                            textEditor.select(actualStartPos, actualEndPos);
                            appendDelegateIndexToSelectedBlocks();
                        } else if (delegate.index === actualStartIndex) {
                            textEditor.select(actualStartPos, textEditor.length);
                            appendDelegateIndexToSelectedBlocks();
                        } else if (delegate.index === actualEndIndex) {
                            textEditor.select(0, actualEndPos);
                            appendDelegateIndexToSelectedBlocks();
                        }

                        if (!(delegate.index < actualStartIndex || delegate.index > actualEndIndex) && actualStartPos !== actualEndPos) {
                            cursorVisible = false;
                        }
                    }

                    function selectWordAtPos(pos) {
                        cursorVisible = false;
                        cursorPosition = pos;
                        selectWord();
                        selectionArea.selStartPos = textEditor.selectionStart;
                        selectionArea.selEndPos = textEditor.selectionEnd;
                        return [selectionStart, selectionEnd];
                    }

                    function findLineStartAndEndPostitionAtPos(str, charPos){
                        let lines = str.split("\u2028"); // Unicode LINE SEPARATOR character
                        let totalChars = 0;

                        for(let i = 0; i < lines.length; i++) {
                            if(totalChars + lines[i].length >= charPos) {
                                return [totalChars, totalChars + lines[i].length];
                            }
                            totalChars += lines[i].length + 1; // +1 for the newline character
                        }

                        return [0, str.length];
                    }

                    function selectLineAtPos(pos) {
                        cursorVisible = false;
                        cursorPosition = pos;
                        const [lineStart, lineEnd] = findLineStartAndEndPostitionAtPos(textEditor.getText(0, textEditor.length), pos);
                        textEditor.select(lineStart, lineEnd);
                        selectionArea.selStartPos = textEditor.selectionStart;
                        selectionArea.selEndPos = textEditor.selectionEnd;
                        return [selectionStart, selectionEnd];
                    }
                }
            }
        }

        ScrollBar.vertical: CustomVerticalScrollBar {
            id: verticalScrollBar
            themeData: root.themeData
            isDarkGray: true
            showBackground: true

            onPositionChanged: {
                saveScrollPositionTimer.restart();
            }
        }

        Timer {
            id: saveScrollPositionTimer
            interval: 500

            onTriggered: {
                BlockModel.setVerticalScrollBarPosition(verticalScrollBar.position, blockEditorView.indexAt(0, blockEditorView.contentY));
            }
        }

        function indexAtRelative(x, y) {
            return indexAt(x + contentX, y + contentY)
        }
    }


    MouseArea {
        id: selectionArea
        propagateComposedEvents: true
        property int selStartIndex: -1
        property int selEndIndex: -1
        property int selStartPos
        property int selEndPos

        property bool isWordSelected: false
        property int wordIndex
        property int wordStartPos
        property int wordEndPos

        property bool canTripleClick: false
        property bool isLineSelected: false
        property int lineIndex
        property int lineStartPos
        property int lineEndPos

        property int scrollTriggerZone: 70
        property int currentScrollDirection: 0 // 0 - no scroll, 1 - scroll up, -1 - scroll down (TODO: change to enum)

        signal selectionChanged

        anchors.fill: parent
        enabled: !verticalScrollBar.hovered
        cursorShape: selectionArea.enabled ? Qt.IBeamCursor : Qt.ArrowCursor

        function indexAndPos(x, y) {
            const index = blockEditorView.indexAtRelative(x, y);
            if (index === -1)
                return [-1, -1];
            const blockDelegate = blockEditorView.itemAtIndex(index);
            const blockDelegateText = blockDelegate.textEditorPointer;
            const relItemY = blockDelegate.y - blockEditorView.contentY;
            const relItemX = selectionArea.mapToItem(blockDelegateText, x, y).x;
            const pos = blockDelegateText.positionAt(relItemX, y - relItemY);
            blockDelegateText.forceActiveFocus();
            blockDelegateText.cursorPosition = pos;
            root.lastCursorPos = pos;
//            console.log("lastCursorPos CHANGED 13:", root.lastCursorPos);
            return [index, pos];
        }

        function isMouseOnTopOfDelegateText (delegateText, mousePoint) {
            var mappedSelectionAreaPointToDelegateText = selectionArea.mapToItem(delegateText, mousePoint);
            return delegateText.contains(mappedSelectionAreaPointToDelegateText);
        }

        onPressed: (mouse) => {
           // TODO: the use of indexAndPos is highly redundant here
            root.enableCursorAnimation = false;
//            animatedCursor.enabledPressAnimation = true;
           const [index, pos] = indexAndPos(mouse.x, mouse.y);
           var blockDelegate = blockEditorView.itemAtIndex(index);
           if (blockDelegate !== null && index !== -1) {
               var blockDelegateText = blockDelegate.textEditorPointer;
               const [index, pos] = indexAndPos(mouse.x, mouse.y);
                if (index !== -1) {
                    root.selectedBlockIndexes = [index];
                    root.selectedBlock = blockEditorView.itemAtIndex(index);
                    console.log("root.selectedBlockIndexes: 16", root.selectedBlockIndexes);
                }
               if (canTripleClick) {
                   if (isMouseOnTopOfDelegateText(blockDelegateText, Qt.point(mouse.x, mouse.y))) {
                       root.isCursorMovedVertically = false;
                       isWordSelected = false;
                       isLineSelected = true;
                       canTripleClick = false;
                       const [index, pos] = indexAndPos(mouse.x, mouse.y);
                       if (index === -1) return;
//                       root.selectedBlockIndexes = [index];
//                        root.selectedBlock = blockEditorView.itemAtIndex(index);
//                                   console.log("root.selectedBlockIndexes: 8", root.selectedBlockIndexes);
                       [lineStartPos, lineEndPos] = blockDelegateText.selectLineAtPos(pos);
                       lineIndex = index;
                   } else {
                       mouse.accepted = false;
                   }
               } else {
                   if (isMouseOnTopOfDelegateText(blockDelegateText, Qt.point(mouse.x, mouse.y))) {
                       root.isCursorMovedVertically = false;
//                       root.selectedBlockIndexes = [index];
//                        root.selectedBlock = blockEditorView.itemAtIndex(index);
//                                   console.log("root.selectedBlockIndexes: 9", root.selectedBlockIndexes);
                       isWordSelected = false;
                       isLineSelected = false;
                       [selStartIndex, selStartPos] = indexAndPos(mouse.x, mouse.y);
                       if (selStartIndex === -1) return;
                       blockDelegateText.cursorVisible = true;
                       [selEndIndex, selEndPos] = [selStartIndex, selStartPos]
                       selectionChanged();

                       if (root.currentHoveredLink !== "") {
                            Qt.openUrlExternally(root.currentHoveredLink);
                       }
                   } else {
                       mouse.accepted = false;
                   }
               }
            }

           if (blockDelegate === null && index === -1 && blockEditorView.count > 0) {
                let lastBlockIndex = blockEditorView.count - 1;
                var lastBlock = blockEditorView.itemAtIndex(lastBlockIndex);
                if (lastBlock !== null && lastBlock.textEditorPointer !== null) {
                    lastBlock.textEditorPointer.cursorPosition = lastBlock.textEditorPointer.length;
                    lastBlock.textEditorPointer.forceActiveFocus();
                    root.selectedBlockIndexes = [lastBlockIndex];
                    root.selectedBlock = blockEditorView.itemAtIndex(lastBlockIndex);
                               console.log("root.selectedBlockIndexes: 10", root.selectedBlockIndexes);
                    selStartIndex = lastBlockIndex;
                    selStartPos = lastBlock.textEditorPointer.length;
                    [selEndIndex, selEndPos] = [selStartIndex, selStartPos]
                    selectionChanged();
                }
            }
        }

        onPositionChanged: (mouse) => {
           if (isLineSelected) {
               const [index, pos] = indexAndPos(mouse.x, mouse.y);
               if (index === -1) return;
               const blockDelegateText = blockEditorView.itemAtIndex(index).textEditorPointer;
               const lineRange = blockDelegateText.selectLineAtPos(pos);
               if (index < lineIndex || (index === lineIndex && lineRange[0] < lineStartPos)) {
                   selStartIndex = index;
                       selStartPos = lineRange[0];
                       selEndIndex = lineIndex;
                       selEndPos = lineEndPos;
                   } else {
                   selStartIndex = lineIndex;
                   selStartPos = lineStartPos;
                   selEndIndex = index;
                   selEndPos = lineRange[1];
               }
           } else if (isWordSelected) {
               const [index, pos] = indexAndPos(mouse.x, mouse.y);
               if (index === -1) return;
               const blockDelegateText = blockEditorView.itemAtIndex(index).textEditorPointer;
               const wordRange = blockDelegateText.selectWordAtPos(pos);
               if (index < wordIndex || (index === wordIndex && wordRange[0] < wordStartPos)) {
                   selStartIndex = index;
                   selStartPos = wordRange[0];
                   selEndIndex = wordIndex;
                   selEndPos = wordEndPos;
               } else {
                   selStartIndex = wordIndex;
                   selStartPos = wordStartPos;
                   selEndIndex = index;
                   selEndPos = wordRange[1];
               }
           } else {
               [selEndIndex, selEndPos] = indexAndPos(mouse.x, mouse.y);
               if (selEndIndex === -1) return;
           }

           selectionChanged();

           if(mouse.y < selectionArea.y + scrollTriggerZone) {
               let relativePos = (selectionArea.y + scrollTriggerZone - mouse.y) / scrollTriggerZone;
               let pixelsPerSec = 200 + relativePos * 1000;
               scrollUpAnimation.duration = Math.max(calculateScrollTimeBasedOnPixelSpeed(-1, pixelsPerSec), 0);
               scrollUpAnimation.restart();
               currentScrollDirection = -1;
           } else if (mouse.y > selectionArea.y + selectionArea.height - scrollTriggerZone) {
               let relativePos = (mouse.y - (selectionArea.y + selectionArea.height - scrollTriggerZone)) / scrollTriggerZone;
               let pixelsPerSec = 200 + relativePos * 1000;
               scrollDownAnimation.duration = Math.max(calculateScrollTimeBasedOnPixelSpeed(1, pixelsPerSec), 0);
               scrollDownAnimation.restart();
               currentScrollDirection = 1;
           } else {
               currentScrollDirection = 0;
           }
       }

        onReleased: {
            currentScrollDirection = 0;
            root.enableCursorAnimation = true;
        }

        PropertyAnimation {
            id: scrollUpAnimation
            target: blockEditorView
            property: "contentY"
            to: 0 - blockEditorView.topMargin
            running: selectionArea.currentScrollDirection === -1
        }

        PropertyAnimation {
            id: scrollDownAnimation
            target: blockEditorView
            property: "contentY"
            to: blockEditorView.contentHeight - blockEditorView.height
            running: selectionArea.currentScrollDirection === 1
        }

        function calculateScrollTimeBasedOnPixelSpeed(direction, pixelsPerSec, accelerationRatio) {
            var pixelsToTravel;
            if(direction === -1) {
                pixelsToTravel = blockEditorView.contentY;
            } else if (direction === 1) {
                pixelsToTravel = Math.abs(blockEditorView.contentHeight - blockEditorView.contentY + blockEditorView.height);
            }
            return pixelsToTravel / pixelsPerSec * 1000;
        }

        onDoubleClicked: (mouse) => {
             selectionArea.canTripleClick = true;
             tripleClickTimer.start();
             const [index, pos] = indexAndPos(mouse.x, mouse.y);
             if (index === -1) return;
             var blockDelegate = blockEditorView.itemAtIndex(index);
             if (blockDelegate !== null) {
                 var blockDelegateText = blockDelegate.textEditorPointer;
                 [wordStartPos, wordEndPos] = blockDelegateText.selectWordAtPos(pos);
                 isWordSelected = true;
                 wordIndex = index;
            }
         }

        Timer {
            id: tripleClickTimer
            interval: 400

            onTriggered: {
                selectionArea.canTripleClick = false;
            }
        }
    }
}
