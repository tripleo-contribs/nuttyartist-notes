#include "blockmodel.h"

BlockModel::BlockModel(QObject *parent)
    : QAbstractListModel{ parent },
      m_sourceDocument(this),
      m_blockList({}),
      m_htmlMetaDataStart(QStringLiteral("<html><head><meta name=\"qrichtext\" content=\"1\" "
                                         "/><meta charset=\"utf-8\" /></head><body>")),
      m_htmlMetaDataEnd(QStringLiteral("</body></html>")),
      m_tabLengthInSpaces(4),
      m_textLineHeightInPercentage(125),
      m_blockIndexToFocusOn(0),
      m_undoStack({}),
      m_redoStack({}),
      m_searchKeyword{""},
      m_searchResultBlockIndexes{{}},
      m_currentTheme(Theme::Light)
{
    Q_UNUSED(parent);
    m_clipboard = QGuiApplication::clipboard();
    m_sourceDocument.setUndoRedoEnabled(false); // We don't need this since we have our own undo/redo system
}

int BlockModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_blockList.length();
}

QVariant BlockModel::data(const QModelIndex &index, int role) const
{
    if (index.isValid() && index.row() >= 0 && index.row() < m_blockList.length()) {
        QSharedPointer<BlockInfo> blockInfo = m_blockList[index.row()];

        switch ((Role)role) {
        case BlockTextHtmlRole:
            return blockInfo->textHtml();
        case BlockTextPlainTextRole:
            return blockInfo->textPlainText();
        case BlockDelimiterRole:
            return blockInfo->blockDelimiter();
        case BlockTypeRole:
            return static_cast<int>(blockInfo->blockType());
        case BlockLineStartPosRole:
            return blockInfo->lineStartPos();
        case BlockLineEndPosRole:
            return blockInfo->lineEndPos();
        case BlockTotalIndentLengthRole:
            return blockInfo->totalIndentLength();
        case BlockIndentLevelRole:
            return blockInfo->indentLevel();
        case BlockChildrenRole:
            return QVariant::fromValue(blockInfo->children());
        case BlockMetaData:
            return QVariant(blockInfo->metaData());
        }
    }

    return {};
}

QHash<int, QByteArray> BlockModel::roleNames() const
{
    QHash<int, QByteArray> result;

    result[BlockTextHtmlRole] = "blockTextHtml";
    result[BlockTextPlainTextRole] = "blockTextPlainText";
    result[BlockTypeRole] = "blockType";
    result[BlockLineStartPosRole] = "blockLineStartPos";
    result[BlockLineEndPosRole] = "blockLineEndPos";
    result[BlockTotalIndentLengthRole] = "blockTotalIndentLength";
    result[BlockIndentLevelRole] = "blockIndentLevel";
    result[BlockDelimiterRole] = "blockDelimiter";
    result[BlockChildrenRole] = "blockChildren";
    result[BlockMetaData] = "blockMetaData";

    return result;
}

QTextDocument *BlockModel::sourceDocument()
{
    return &m_sourceDocument;
}

void BlockModel::processOutput(const MD_CHAR *output, MD_SIZE size, void *userdata)
{
    QByteArray *buffer = reinterpret_cast<QByteArray *>(userdata);
    buffer->append(output, size);
}

QString BlockModel::markdownToHtml(const QString &markdown)
{
    QByteArray md = markdown.toUtf8();
    QByteArray html;

    unsigned parser_flags = MD_FLAG_STRIKETHROUGH | MD_FLAG_NOINDENTEDCODEBLOCKS;

    if (md_html((MD_CHAR *)md.data(), md.size(), &processOutput, (void *)&html, parser_flags, 0)
        == 0)
        return QString::fromUtf8(html);
    else
        return QString(); // or throw an exception
}

// TODO: this function should be in BlockInfo not here
unsigned int BlockModel::calculateTotalIndentLength(const QString &str,
                                                    QSharedPointer<BlockInfo> &blockInfo)
{
    unsigned int totalIndentLength = 0;
    QString indentedString = "";
    for (auto &c : str) {
        if (c == '\t') {
            totalIndentLength += m_tabLengthInSpaces;
            indentedString += c;
        } else if (c == ' ') {
            totalIndentLength += 1;
            indentedString += c;
        } else {
            break;
        }
    }
    blockInfo->setIndentedString(indentedString); // TODO: this should not be here
    return totalIndentLength;
}

// TODO: this function should be inside BlockInfo, not BlockModel
void BlockModel::updateBlockText(QSharedPointer<BlockInfo> &blockInfo, const QString &plainText,
                                 unsigned int lineStartPos, unsigned int lineEndPos)
{
    // Plain text
    blockInfo->setTextPlainText(plainText);

    // HTML
    // We clean the plain text string from:
    // 1. Indentation at the start of the string
    // 2. Delimeter
    QString htmlOutput = plainText.mid(blockInfo->indentedString().length()
                                       + blockInfo->blockDelimiter().length());

    if (!m_searchKeyword.isEmpty()) {
        int indexFound = htmlOutput.indexOf(m_searchKeyword, 0, Qt::CaseInsensitive);
        while (indexFound != -1) {
            QString savedKeyword = htmlOutput.mid(indexFound, m_searchKeyword.length());
            QString newText = QStringLiteral("<span style=\"background-color: ") + (m_currentTheme == Theme::Dark ? "#454d56" : "#d3e6fb") + QStringLiteral("\">") + savedKeyword + "</span>";
            htmlOutput.replace(indexFound, savedKeyword.length(), newText);
            indexFound = htmlOutput.indexOf(m_searchKeyword, indexFound + savedKeyword.length() + newText.length() - 1, Qt::CaseInsensitive);
            m_searchResultBlockIndexes << m_blockList.indexOf(blockInfo);
        }
    }

//        qDebug() << "htmlOutput 0: " << htmlOutput;
    //    qDebug() << "BlockType (in updateBlockText): " << blockInfo->blockType();
    //    qDebug() << "htmlOutput 1: " << htmlOutput;
    htmlOutput = m_htmlMetaDataStart
            + (htmlOutput.length() < 2 ? htmlOutput : markdownToHtml(htmlOutput))
            + m_htmlMetaDataEnd;
//        qDebug() << "htmlOutput 2: " << htmlOutput;
    if (blockInfo->blockType() != BlockInfo::BlockType::Heading) {
        htmlOutput.replace(
                "<p>",
                QStringLiteral("<p style=\"line-height:%1%;\">").arg(m_textLineHeightInPercentage));
        if (!htmlOutput.contains("</p>"))
            htmlOutput.replace("<body>",
                               QStringLiteral("<body style=\"line-height:%1%;\">")
                                       .arg(m_textLineHeightInPercentage));
//                qDebug() << "htmlOutput 3: " << htmlOutput;
    }
    blockInfo->setTextHtml(htmlOutput);

    // Lines
    blockInfo->setLineStartPos(lineStartPos);
    blockInfo->setLineEndPos(lineEndPos);
}

void BlockModel::determineBlockIndentAndParentChildRelationship(
        QSharedPointer<BlockInfo> &blockInfo, int positionToStartSearchFrom)
{
    //    qDebug() << "In determineBlockIndentAndParentChildRelationship";
    if (positionToStartSearchFrom < 1 || positionToStartSearchFrom > m_blockList.length() - 1)
        return;

    if (blockInfo->totalIndentLength() == 0) {
        if (blockInfo->parent() != nullptr)
            blockInfo->parent()->removeChild(blockInfo);
        //        blockInfo->setParent(nullptr);
        return;
    }

    //    qDebug() << "here 0";

    // We traverse the list backwards to check if this block has a parent
    for (int i = positionToStartSearchFrom; i >= 0; i--) {
        QSharedPointer<BlockInfo> previousBlock = m_blockList[i];
        //        qDebug() << "previousBlock->totalIndentLength(): " <<
        //        previousBlock->totalIndentLength();

        // If previousBlock has a smaller total indent length than the current line/block
        // We assign it has a parent for the current block
        if (previousBlock->totalIndentLength() < blockInfo->totalIndentLength()) {

            //            qDebug() << "here 1";

            // If parent is changing, we remove this block from its children
            if (blockInfo->parent() != nullptr) { //&& blockInfo->parent() != previousBlock) {
                blockInfo->parent()->removeChild(blockInfo);
                //                qDebug() << "here 2";
                //                qDebug() << "parent text: " <<
                //                blockInfo->parent()->textPlainText();
            }

            blockInfo->setIndentLevel(previousBlock->indentLevel() + 1);
            //            qDebug() << "previousBlock->indentLevel() + 1: " <<
            //            previousBlock->indentLevel() + 1;
            blockInfo->setParent(previousBlock);
            previousBlock->addChild(blockInfo);
            //            previousBlock->addChild(blockInfo);
            break;
        }
    }
}

void BlockModel::updateBlockUsingPlainText(QSharedPointer<BlockInfo> &blockInfo,
                                           unsigned int blockIndex, QString &plainText)
{
    //    qDebug() << "In updateBlockUsingPlainText";
    //    qDebug() << "plainText: " << plainText;
    unsigned int lineTotalIndentLength = calculateTotalIndentLength(plainText, blockInfo);
    blockInfo->setTotalIndentLength(lineTotalIndentLength);
    blockInfo->determineBlockType(plainText);
    if (blockInfo->blockType() == BlockInfo::BlockType::Divider)
        plainText = blockInfo->blockDelimiter();
    int lastPos = blockIndex > 0 ? m_blockList[blockIndex - 1]->lineEndPos() : -1;
    updateBlockText(blockInfo, plainText, lastPos + 1, lastPos + 1);
    blockInfo->setIndentLevel(0);
    if (blockInfo->parent() != nullptr) {
        blockInfo->parent()->removeChild(blockInfo);
        //        if (lineTotalIndentLength <= 0)
        //            blockInfo->setParent(nullptr);
    }
    //    blockInfo->setParent(nullptr);
    if (lineTotalIndentLength > 0 && m_blockList.length() > 0) {
        determineBlockIndentAndParentChildRelationship(blockInfo, blockIndex - 1);
    }
}

void BlockModel::loadText(const QString &text)
{
    QElapsedTimer timer;
    timer.start();
    clear();

    emit aboutToLoadText();
    m_sourceDocument.setPlainText(text); // 40% performance hit
    QStringList lines = text.split("\n"); // TODO: Should we use Qt::SkipEmptyParts?

    beginInsertRows(QModelIndex(), 0, lines.size() - 1);

    unsigned int blockIndex = 0;
    for (auto &line : lines) {
        QSharedPointer<BlockInfo> blockInfo = QSharedPointer<BlockInfo>(new BlockInfo());
        m_blockList << blockInfo;
        updateBlockUsingPlainText(blockInfo, blockIndex, line);
        blockIndex++;
    }

    endInsertRows();
    qDebug() << "Finished loading.";

    QJsonObject dataToSendToView;
    if (m_searchResultBlockIndexes.isEmpty()) {
        dataToSendToView["itemIndexInView"] = m_itemIndexInView;
    } else {
        dataToSendToView["itemIndexInView"] = m_searchResultBlockIndexes[0];
    }
    emit loadTextFinished(QVariant(dataToSendToView));

    qint64 elapsed = timer.elapsed();
    qDebug() << "Time taken:" << elapsed << "ms";
}

void BlockModel::clear()
{
    beginResetModel();
    m_searchResultBlockIndexes.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_blockList.clear();
    endResetModel();
}

void BlockModel::setNothingLoaded()
{
    clear();
    emit nothingLoaded();
}

void BlockModel::updateBlocksLinePositions(unsigned int blockPosition, int delta)
{
    for (unsigned int i = blockPosition; i < m_blockList.length(); i++) {
        QSharedPointer<BlockInfo> blockInfo = m_blockList[i];
        //        qDebug() << "updated block plaintext: " << blockInfo->textPlainText();
        //        qDebug() << "new lineStartPos: " << blockInfo->lineStartPos() + delta;
        //        qDebug() << "new lineEndPos: " << blockInfo->lineEndPos() + delta;
        blockInfo->setLineStartPos(blockInfo->lineStartPos() + delta);
        blockInfo->setLineEndPos(blockInfo->lineEndPos() + delta);
    }
}

QString BlockModel::QmlHtmlToMarkdown(QString &qmlHtml)
{
    // Converting QML's TextArea text (HTML) to Markdown (Plaintext)
    //    qDebug() << "qmlHtml: " << qmlHtml;
    qmlHtml.replace("<br />", "\u2063"); // To preserve <br /> after toMarkdown() (otherwise it will
                                         // parse these as line breaks)
    QTextDocument doc;
    doc.setHtml(qmlHtml);
//    QString markdown = doc.toMarkdown();
    //    qDebug() << "markdown 0: " << markdown;
//    markdown.replace("\u2063", "<br />");
//    markdown.replace("\n\n", ""); // due to toMarkdown() adding these
//    markdown.replace("\n", " "); // due to QML's TextEdit text wrapping adding these
//    //    qDebug() << "markdown 1: " << markdown;

//    return markdown;

    return doc.toMarkdown()
            .replace("\u2063", "<br />") // restore <br />s
            .replace("\n\n", "") // due to toMarkdown() adding these
            .replace("\n", " ");; // due to QML's TextEdit text wrapping adding these
}

void BlockModel::setTextAtIndex(const int blockIndex, QString qmlHtml, int cursorPositionQML)
{
    emit aboutToChangeText();

    QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];

    QString markdown = QmlHtmlToMarkdown(qmlHtml);
    // Preserve delimiter
    markdown = blockInfo->blockDelimiter() + markdown;
    // Preserve indentation
    if (blockInfo->totalIndentLength() > 0)
        markdown = blockInfo->indentedString() + markdown;

    if (blockInfo->textPlainText() == markdown) {
        emit textChangeFinished();
        return;
    }

    if (abs(blockInfo->textPlainText().length() - markdown.length()) == 1) {
        // one char operation
        OneCharOperation oneCharOperation = blockInfo->textPlainText().length() > markdown.length()
                ? OneCharOperation::CharDelete
                :  OneCharOperation::CharInsert;
        updateSourceTextBetweenLines(blockInfo->lineStartPos(), blockInfo->lineEndPos(), markdown,
                                     true, cursorPositionQML, ActionType::Modify, oneCharOperation);
    } else {
        // not one char operation
        updateSourceTextBetweenLines(blockInfo->lineStartPos(), blockInfo->lineEndPos(), markdown,
                                     true, cursorPositionQML, ActionType::Modify,
                                     OneCharOperation::NoOneCharOperation);
    }

    blockInfo->determineBlockType(markdown);
    int numberOfLinesBefore = blockInfo->lineEndPos() - blockInfo->lineStartPos() + 1;
    int numberOfLinesDelta = markdown.count('\n') + 1 - numberOfLinesBefore; // TODO: probably unnecessary
    updateBlockText(blockInfo, markdown, blockInfo->lineStartPos(),
                    blockInfo->lineEndPos() + numberOfLinesDelta);
    if (numberOfLinesDelta != 0) { // TODO: probably unnecessary
        updateBlocksLinePositions(blockIndex + 1, numberOfLinesDelta);
    }

    QModelIndex modelIdx = this->index(blockIndex);
    emit dataChanged(modelIdx, modelIdx, {}); // TODO: Empty vector means all roles, is that good?
    emit textChangeFinished();
}

// TODO: This function slows the app down (writing lag) when the text is very large (e.g. Moby Dick)
// And we don't really need to rely on QTextDocument for that
void BlockModel::updateSourceTextBetweenLines(
        int startLinePos, int endLinePos, const QString &newText, bool shouldCreateUndo,
        int cursorPositionQML, ActionType actionType, OneCharOperation oneCharoperation,
        bool isForceMergeLastAction, int firstBlockSelectionStart, int lastBlockSelectionEnd)
{
    //    qDebug() << "startLinePos: " << startLinePos;
    //    qDebug() << "endLinePos: " << endLinePos;
    //    qDebug() << "actionType: " << actionType;
    //    qDebug() << "sourceDocument before: " << m_sourceDocument.toPlainText();

    SingleAction singleAction;
    bool isMergingLastAction = isForceMergeLastAction;

    if (shouldCreateUndo) {
        m_redoStack.clear(); // Clear redo stack after edit
        singleAction.actionType = actionType;
        singleAction.blockStartIndex = startLinePos;
        singleAction.blockEndIndex = endLinePos;
        singleAction.newPlainText = newText;
        singleAction.oneCharOperation = oneCharoperation;
        singleAction.lastCursorPosition = cursorPositionQML;

        if (actionType == ActionType::Modify) {
            // Modifying only one block
            if (!m_undoStack.isEmpty() && !m_undoStack.last().actions.isEmpty()) {
                SingleAction &lastAction = m_undoStack.last().actions.last();
                if (oneCharoperation == OneCharOperation::CharInsert
                    || oneCharoperation == OneCharOperation::CharDelete) {
                    if (lastAction.oneCharOperation == oneCharoperation
                        && lastAction.blockStartIndex == startLinePos
                        && lastAction.blockEndIndex == endLinePos
                        && abs(lastAction.lastCursorPosition - cursorPositionQML) == 1) {

                        //                        qDebug() << "Merging last char action";
                        isMergingLastAction = true;
                        lastAction.lastCursorPosition = cursorPositionQML;
                        lastAction.newPlainText = newText;
                    }
                }
            }
        }

        if (actionType == ActionType::Remove) {
            singleAction.firstBlockSelectionStart = firstBlockSelectionStart;
            singleAction.lastBlockSelectionEnd = lastBlockSelectionEnd;
        }
    }

    QTextBlock startBlock = m_sourceDocument.findBlockByLineNumber(startLinePos);
    QTextBlock endBlock = m_sourceDocument.findBlockByLineNumber(endLinePos);
    QTextCursor cursor(startBlock);
    cursor.setPosition(endBlock.position() + endBlock.length() - 1, QTextCursor::KeepAnchor);
    singleAction.oldPlainText = cursor.selectedText().replace(
            "\u2029", "\n"); // In selectedText(), line breaks '\n' are replaced with Unicode
                             // U+2029. See: https://doc.qt.io/qt-6/qtextcursor.html#selectedText

    if (actionType == ActionType::Modify) {
        cursor.removeSelectedText();
        cursor.insertText(newText);
    } else if (actionType == ActionType::Remove) {
        //        qDebug() << "remove startLinePos: " << startLinePos;
        int positionToRemoveFrom = startBlock.position()
                - (startBlock.position() > 0 && startLinePos > 0
                           ? 1
                           : 0); // we want to remove the newline character at the start as well, if
                                 // exist
        cursor.setPosition(positionToRemoveFrom, QTextCursor::MoveAnchor);
        cursor.setPosition(endBlock.position() + endBlock.length() - 1, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    } else if (actionType == ActionType::Insert) {
        //        qDebug() << "startLinePos: " << startLinePos;
        //        qDebug() << "startBlock.position(): " << startBlock.position();
        //        qDebug() << "m_sourceDocument.lineCount() - 1: " << m_sourceDocument.lineCount() -
        //        1; qDebug() << "startBlock.position(): " << startBlock.position(); qDebug() <<
        //        "characterCount: " << m_sourceDocument.characterCount();
        if (startLinePos > m_sourceDocument.lineCount() - 1) {
            //            qDebug() << "inserting at the end of the document";
            cursor.setPosition(m_sourceDocument.characterCount() - 1, QTextCursor::MoveAnchor);
        } else {
            //            qDebug() << "inserting at the middle of the document";
            cursor.setPosition(startBlock.position(), QTextCursor::MoveAnchor);
        }
        if (startLinePos > m_sourceDocument.lineCount() - 1) {
            // If we're inserting at the end of the document
            //            qDebug() << "inserting newline";
            cursor.insertText("\n" + newText);
        } else {
            // If we're inserting at the middle of the document
            //            qDebug() << "not inserting newline";
            cursor.insertText(newText + "\n");
        }
    }

    if (isForceMergeLastAction && !m_undoStack.isEmpty()) {
        //        m_undoStack.last().actions.push_back(singleAction);
        m_undoStack.last().actions.push_front(singleAction);
    }

    if (!isMergingLastAction && shouldCreateUndo) {
        //        qDebug() << "new CompoundAction";
        CompoundAction compoundAction;
        //        compoundAction.actions.push_back(singleAction);
        compoundAction.actions.push_front(singleAction);
        m_undoStack.push_back(compoundAction);
        //        qDebug() << "m_undoStack in KB: " << estimateMemoryUsageInKB(m_undoStack);
    }

    //    qDebug() << "sourceDocument after: " << m_sourceDocument.toPlainText();
}

double BlockModel::estimateMemoryUsageInKB(const QList<CompoundAction> &undoStack)
{
    int totalSize = 0;
    // Size of QList overhead
    totalSize += sizeof(QList<CompoundAction>);
    for (const CompoundAction &compoundAction : undoStack) {
        // Size of QList<SingleAction> overhead
        totalSize += sizeof(QList<SingleAction>);
        for (const SingleAction &action : compoundAction.actions) {
            // Size of two unsigned int and one enum (assuming it's sizeof(int))
            totalSize += 2 * sizeof(unsigned int) + sizeof(int);
            // Size of QString overhead and its content
            totalSize += 2 * sizeof(QString) + action.oldPlainText.size() * sizeof(QChar)
                    + action.newPlainText.size() * sizeof(QChar);
            // Size of OneCharOperation
            totalSize += sizeof(OneCharOperation);
            // Size of int
            totalSize += sizeof(int);
        }
    }

    // Convert totalSize (in bytes) to kilobytes
    return static_cast<double>(totalSize) / 1024.0;
}

void BlockModel::indentBlocks(QList<int> selectedBlockIndexes)
{
    std::sort(selectedBlockIndexes.begin(), selectedBlockIndexes.end());

    if (selectedBlockIndexes.length() > 0) {
        emit aboutToChangeText();
        // Running on the selected blocks to check for indentation
        int numberOfAlreadyIndentedBlocks = 0;
        for (int i = selectedBlockIndexes[0];
             i < selectedBlockIndexes[selectedBlockIndexes.length() - 1] + 1; i++) {
            QSharedPointer<BlockInfo> blockInfo = m_blockList[i];
            if (blockInfo->isIndentable() && i > 0) {

                // Traverse the list backwards from current block position to check for a matching
                // parent. If none found (or an empty block encountered), nothing is done.
                for (int j = i - 1; j >= 0; j--) {
                    QSharedPointer<BlockInfo> previousBlock = m_blockList[j];

                    if (previousBlock->indentLevel() < blockInfo->indentLevel()) {
                        break;
                    }

                    if (previousBlock->indentLevel() == blockInfo->indentLevel()) {

                        QString newIndentedString = previousBlock->indentedString() + '\t';
                        QString newIndentedPlainText = blockInfo->textPlainText();
                        newIndentedPlainText = newIndentedPlainText.sliced(
                                blockInfo->indentedString().length(),
                                newIndentedPlainText.length()
                                        - blockInfo->indentedString().length());
                        newIndentedPlainText = newIndentedString + newIndentedPlainText;

                        blockInfo->setIndentedString(newIndentedString);
                        blockInfo->setTotalIndentLength(previousBlock->totalIndentLength()
                                                        + m_tabLengthInSpaces);
                        blockInfo->setIndentLevel(blockInfo->indentLevel() + 1);

                        updateSourceTextBetweenLines(
                                blockInfo->lineStartPos(), blockInfo->lineEndPos(),
                                newIndentedPlainText, true, 0, ActionType::Modify,
                                OneCharOperation::Indent,
                                numberOfAlreadyIndentedBlocks <= 0 ? false : true);
                        numberOfAlreadyIndentedBlocks++; // We want only the first indented block to
                                                         // create an undo compund action, the rest
                                                         // will be merged with it

                        updateBlockText(blockInfo, newIndentedPlainText, blockInfo->lineStartPos(),
                                        blockInfo->lineEndPos());

                        if (blockInfo->parent() != nullptr) {
                            blockInfo->parent()->removeChild(blockInfo);
                        }
                        blockInfo->setParent(previousBlock);
                        previousBlock->addChild(blockInfo);

                        // We need to run on all the children and ask them to reavaluate their
                        // parent
                        for (auto &child : blockInfo->children()) {
                            determineBlockIndentAndParentChildRelationship(
                                    child, m_blockList.indexOf(child) - 1);
                        }

                        //                        previousBlock->addChild(blockInfo);

                        QModelIndex modelIdx = this->index(i);
                        emit dataChanged(modelIdx, modelIdx, {});

                        break;
                    }
                }
            }
        }
        emit textChangeFinished();
    }
}

void BlockModel::unindentBlock(unsigned int blockIndex, QSharedPointer<BlockInfo> &blockInfo,
                               bool isSecondRun, int numberOfAlreadyUnindentedBlocks)
{
    qDebug() << "unindentBlock!";
    emit aboutToChangeText();
    QString plainTextWithoutIndentation =
            blockInfo->textPlainText().mid(blockInfo->indentedString().length());
    //    qDebug() << "is parent null: " << (blockInfo->parent() == nullptr);
    //    qDebug() << "parent text: " << blockInfo->parent()->textPlainText();
    //    qDebug() << "parent indentedString: " << blockInfo->parent()->indentedString();
    QString newPlainText = blockInfo->parent()->indentedString() + (isSecondRun ? "\t" : "")
            + plainTextWithoutIndentation;

    blockInfo->setIndentedString(blockInfo->parent()->indentedString() + (isSecondRun ? "\t" : ""));
    blockInfo->setTotalIndentLength(blockInfo->parent()->totalIndentLength()
                                    + (isSecondRun ? m_tabLengthInSpaces : 0));
    blockInfo->setIndentLevel(blockInfo->parent()->indentLevel() + (isSecondRun ? 1 : 0));

    determineBlockIndentAndParentChildRelationship(blockInfo, blockIndex - 1);

    updateSourceTextBetweenLines(blockInfo->lineStartPos(), blockInfo->lineEndPos(), newPlainText,
                                 true, 0, ActionType::Modify, OneCharOperation::Unindent,
                                 numberOfAlreadyUnindentedBlocks <= 0 ? false : true);

    updateBlockText(blockInfo, newPlainText, blockInfo->lineStartPos(), blockInfo->lineEndPos());

    QModelIndex modelIdx = this->index(blockIndex);
    emit dataChanged(modelIdx, modelIdx, {});
    emit textChangeFinished();
}

void BlockModel::unindentBlocks(QList<int> selectedBlockIndexes)
{
    //    qDebug() << "Unindent: " << selectedBlockIndexes;
    QList<QSharedPointer<BlockInfo>> unindentedAlready{};
    std::sort(selectedBlockIndexes.begin(), selectedBlockIndexes.end());

    if (selectedBlockIndexes.length() > 0) {
        for (int i = selectedBlockIndexes[0];
             i < selectedBlockIndexes[selectedBlockIndexes.length() - 1] + 1; i++) {
            QSharedPointer<BlockInfo> blockInfo = m_blockList[i];
            if (blockInfo->isIndentable() && i > 0) {
                //                qDebug() << "1";
                //                qDebug() << blockInfo->indentLevel();
                //                qDebug() << bool(blockInfo->parent() == nullptr);
                unsigned int originalBlockIndentLevel = blockInfo->indentLevel();
                if (blockInfo->indentLevel() > 0 && blockInfo->parent() != nullptr
                    && !unindentedAlready.contains(blockInfo)) {
                    //                    qDebug() << "2";
                    unindentBlock(i, blockInfo, false, unindentedAlready.length());
                    unindentedAlready.push_back(blockInfo);
                    int j = i + 1;
                    while (j < m_blockList.length()) {
                        //                        qDebug() << "3";
                        QSharedPointer<BlockInfo> nextBlock = m_blockList[j];

                        if (nextBlock->indentLevel() <= originalBlockIndentLevel) {
                            break;
                        }

                        //                        qDebug() << "4";

                        if (nextBlock->indentLevel() > 0 && nextBlock->parent() != nullptr
                            && nextBlock->indentLevel() > originalBlockIndentLevel
                            && !unindentedAlready.contains(nextBlock)) {
                            //                            qDebug() << "4.5";
                            unindentBlock(j, nextBlock, true, unindentedAlready.length());
                            unindentedAlready.push_back(nextBlock);
                        }

                        //                        qDebug() << "5";

                        j++;
                    }
                }
            }
        }
    }
}

void BlockModel::moveBlockTextToBlockAbove(int blockIndex)
{
    if (blockIndex > 0) {
        QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];
        QSharedPointer<BlockInfo> previousBlock = m_blockList[blockIndex - 1];

        emit aboutToChangeText();

        updateSourceTextBetweenLines(previousBlock->lineStartPos(), previousBlock->lineEndPos(),
                                     previousBlock->textPlainText() + blockInfo->textPlainText());

        updateBlockText(previousBlock, previousBlock->textPlainText() + blockInfo->textPlainText(),
                        previousBlock->lineStartPos(), previousBlock->lineEndPos());

        QModelIndex modelIdx = this->index(blockIndex - 1);
        emit dataChanged(modelIdx, modelIdx, {});

        updateBlocksLinePositions(blockIndex + 1, -1); // decrease all blocks lines data by 1

        emit textChangeFinished();
    }
}

void BlockModel::backSpacePressedAtStartOfBlock(int blockIndex)
{
    QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];

    if (blockInfo->blockType() != BlockInfo::BlockType::RegularText
        && !blockInfo->blockDelimiter().isEmpty()) {
        // if this block has a delimiter -> we remove it
        emit aboutToChangeText();

        QString blockPlainText = blockInfo->textPlainText();
        QString afterIndentRemoval = blockPlainText.mid(blockInfo->indentedString().length());
        QString newBlockPlainText = afterIndentRemoval.mid(blockInfo->blockDelimiter().length());
        newBlockPlainText = blockInfo->indentedString() + newBlockPlainText;

        //        updateSourceTextBetweenLines(blockInfo->lineStartPos(),
        //                                     blockInfo->lineEndPos(),
        //                                     newBlockPlainText);

        updateSourceTextBetweenLines(blockInfo->lineStartPos(), blockInfo->lineEndPos(),
                                     newBlockPlainText, true, 0, ActionType::Modify,
                                     OneCharOperation::NoOneCharOperation, false);

        blockInfo->determineBlockType(newBlockPlainText);

        updateBlockText(blockInfo, newBlockPlainText, blockInfo->lineStartPos(),
                        blockInfo->lineEndPos());

        QModelIndex modelIdx = this->index(blockIndex);
        emit dataChanged(modelIdx, modelIdx, {});
        emit textChangeFinished();
    } else if (blockInfo->indentLevel() > 0) {
        // if this block is a regularText but indented -> we unindent it
        unindentBlocks({ blockIndex });
    } else if (blockIndex > 0) {
        // if this block is a regularText and not indented -> we remove it
        emit aboutToChangeText();

        updateSourceTextBetweenLines(blockInfo->lineStartPos(), blockInfo->lineEndPos(), "", true,
                                     0, ActionType::Remove, OneCharOperation::NoOneCharOperation,
                                     true);
        emit textChangeFinished();
        beginRemoveRows(QModelIndex(), blockIndex, blockIndex);
        // We need to run on all the children and ask them to reavaluate their parent
        m_blockList.removeAt(blockIndex);
        if (blockInfo->parent() != nullptr) {
            blockInfo->parent()->removeChild(blockInfo);
            //            blockInfo->setParent(nullptr);
        }
        for (auto &child : blockInfo->children()) {
            if (child->parent() != nullptr)
                child->parent()->removeChild(child);
            //            child->setParent(nullptr);
            determineBlockIndentAndParentChildRelationship(child, blockIndex - 1);
        }
        //        blockInfo->deleteLater();
        endRemoveRows();
    }
}

void BlockModel::insertNewBlock(int blockIndex, QString qmlHtml, bool shouldMergeWithPreviousAction)
{
    qDebug() << "In insertNewBlock";
    emit aboutToChangeText();
    beginInsertRows(QModelIndex(), blockIndex + 1, blockIndex + 1);
    QSharedPointer<BlockInfo> previousBlockInfo = m_blockList[blockIndex];
    //    BlockInfo *newBlockInfo = new BlockInfo(this);
    QSharedPointer<BlockInfo> newBlockInfo = QSharedPointer<BlockInfo>(new BlockInfo());

    newBlockInfo->setTotalIndentLength(previousBlockInfo->totalIndentLength());
    newBlockInfo->setIndentedString(previousBlockInfo->indentedString());
    newBlockInfo->determineBlockType(
            previousBlockInfo->textPlainText()); // TODO: set block type and delimteter without this

    QString markdown = qmlHtml.isEmpty() ? QStringLiteral("") : QmlHtmlToMarkdown(qmlHtml);
    // If the previous block is an item in a numbered list we increase the delimiter's number by 1
    if (newBlockInfo->blockType() == BlockInfo::BlockType::NumberedListItem) {
        int indexOfDotInDelimiter = newBlockInfo->blockDelimiter().indexOf(".");
        if (indexOfDotInDelimiter != -1) {
            int previousBlockNumber =
                    newBlockInfo->blockDelimiter().sliced(0, indexOfDotInDelimiter).toInt();
            newBlockInfo->setBlockDelimiter(QString::number(previousBlockNumber + 1) + ". ");
        }
    }

    // If the previous block is a checked task item, we need to make this an uncheked task
    if (previousBlockInfo->blockType() == BlockInfo::BlockType::Todo
        && previousBlockInfo->metaData()["taskChecked"].toBool()) {
        QString newTaskDelimiter = previousBlockInfo->blockDelimiter();
        newTaskDelimiter.replace("x", " ");
        newTaskDelimiter.replace("X", " ");
        newBlockInfo->setBlockDelimiter(newTaskDelimiter);
        newBlockInfo->updateMetaData("taskChecked", false);
    }

    // If the previous block is a divider or a quote and an enter is pressed, we simply create a
    // regular empty block
    if (previousBlockInfo->blockType() == BlockInfo::BlockType::Divider
        || previousBlockInfo->blockType() == BlockInfo::BlockType::Quote
        || previousBlockInfo->blockType() == BlockInfo::BlockType::DropCap) {
        newBlockInfo->setBlockDelimiter("");
        newBlockInfo->setBlockType(BlockInfo::BlockType::RegularText);
        newBlockInfo->setTextPlainText("");
        markdown = "";
    }

    // Preserve delimiter
    markdown = newBlockInfo->blockDelimiter() + markdown;
    // Preserve indentation
    //    qDebug() << "newBlockInfo->totalindentlength: " << newBlockInfo->totalIndentLength();
    if (newBlockInfo->totalIndentLength() > 0)
        markdown = newBlockInfo->indentedString() + markdown;

    //    qDebug() << "previousBlockInfo->lineEndPos() + 1: " << previousBlockInfo->lineEndPos() +
    //    1; qDebug() << "shouldMergeWithPreviousAction: " << shouldMergeWithPreviousAction;

    updateSourceTextBetweenLines(previousBlockInfo->lineEndPos() + 1,
                                 previousBlockInfo->lineEndPos() + 1, markdown, true, 0,
                                 ActionType::Insert, OneCharOperation::NoOneCharOperation,
                                 shouldMergeWithPreviousAction);

    updateBlockText(newBlockInfo, markdown, previousBlockInfo->lineEndPos() + 1,
                    previousBlockInfo->lineEndPos() + 1);

    //    qDebug() << "newBlockInfo->lineStartPos(): " << newBlockInfo->lineStartPos();
    //    qDebug() << "newBlockInfo->lineEndPos(): " << newBlockInfo->lineEndPos();

    newBlockInfo->setIndentLevel(previousBlockInfo->indentLevel());
    //    newBlockInfo->setParent(nullptr);
    if (newBlockInfo->totalIndentLength() > 0 && m_blockList.length() > 0) {
        //        qDebug() << "DDDDDDDDDDDDD";
        determineBlockIndentAndParentChildRelationship(newBlockInfo, blockIndex);
    }
    emit newBlockCreated(blockIndex + 1);
    m_blockList.insert(blockIndex + 1, newBlockInfo);
    endInsertRows();
    emit textChangeFinished();

    updateBlocksLinePositions(blockIndex + 2, 1);
}

BlockInfo::BlockType BlockModel::getBlockType(int blockIndex)
{
    if (blockIndex < 0 || blockIndex > m_blockList.length() - 1)
        return BlockInfo::BlockType::RegularText;

    QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];
    return blockInfo->blockType();
}

void BlockModel::toggleTaskAtIndex(int blockIndex)
{
    emit aboutToChangeText();
    QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];
    if (blockInfo->blockType() != BlockInfo::BlockType::Todo)
        return;

    QString newDelimiter = blockInfo->blockDelimiter();
    if (blockInfo->blockDelimiter().contains('x')) {
        newDelimiter.replace("x", " ");
        blockInfo->updateMetaData("taskChecked", false);
        //        qDebug() << "taskChecked: " << false;
    } else if (blockInfo->blockDelimiter().contains('X')) {
        newDelimiter.replace("X", " ");
        blockInfo->updateMetaData("taskChecked", false);
        //        qDebug() << "taskChecked: " << false;
    } else {
        newDelimiter.replace("[ ]", "[x]");
        blockInfo->updateMetaData("taskChecked", true);
        //        qDebug() << "taskChecked: " << true;
    }
    blockInfo->setBlockDelimiter(newDelimiter);

    QString plainText = blockInfo->textPlainText().mid(blockInfo->indentedString().length()
                                                       + blockInfo->blockDelimiter().length());
    // New delimiter
    plainText = blockInfo->blockDelimiter() + plainText;
    // Preserve indentation
    if (blockInfo->totalIndentLength() > 0)
        plainText = blockInfo->indentedString() + plainText;

    updateBlockText(blockInfo, plainText, blockInfo->lineStartPos(), blockInfo->lineEndPos());

    updateSourceTextBetweenLines(blockInfo->lineStartPos(), blockInfo->lineEndPos(), plainText,
                                 true, 0, ActionType::Modify, OneCharOperation::NoOneCharOperation,
                                 false);

    QModelIndex modelIdx = this->index(blockIndex);
    emit dataChanged(modelIdx, modelIdx, {});
    emit textChangeFinished();
}

void BlockModel::editBlocks(QList<int> selectedBlockIndexes, int firstBlockSelectionStart,
                            int lastBlockSelectionEnd, int savedPressedChar,
                            bool isPressedCharLower)
{
    if (selectedBlockIndexes.length() < 2)
        return;

    emit aboutToChangeText();
    std::sort(selectedBlockIndexes.begin(), selectedBlockIndexes.end());

    //    qDebug() << "firstBlockSelectionStart: " << firstBlockSelectionStart;
    //    qDebug() << "lastBlockSelectionEnd: " << lastBlockSelectionEnd;

    QSharedPointer<BlockInfo> firstBlock = m_blockList[selectedBlockIndexes[0]];
    int lastBlockIndex = selectedBlockIndexes[selectedBlockIndexes.length() - 1];
    QSharedPointer<BlockInfo> lastBlock = m_blockList[lastBlockIndex];
    QString savedTextFirstBlock = firstBlock->textPlainText().mid(
            firstBlock->indentedString().length() + firstBlock->blockDelimiter().length());
    //    qDebug() << "savedTextFirstBlock 1: " << savedTextFirstBlock;
    savedTextFirstBlock = firstBlock->indentedString() + firstBlock->blockDelimiter()
            + savedTextFirstBlock.mid(0, firstBlockSelectionStart);
    //    qDebug() << "savedTextFirstBlock 2: " << savedTextFirstBlock;
    //    qDebug() << "lastBlock->textPlainText: " << lastBlock->textPlainText();
    //    qDebug() << "lastBlock->textPlainText length: " << lastBlock->textPlainText().length();
    int lineBreakCount = lastBlock->textPlainText().count("<br />");
    int lineBreaksLength = (lineBreakCount * QStringLiteral("<br />").length()) - lineBreakCount;
    //    qDebug() << "lastBlock->indentedString().length(): " <<
    //    lastBlock->indentedString().length(); qDebug() << "lastBlock->blockDelimiter().length(): "
    //    << lastBlock->blockDelimiter().length(); qDebug() << "lastBlockSelectionEnd: " <<
    //    lastBlockSelectionEnd; qDebug() << "lineBreaksLength: " << lineBreaksLength;
    QString savedTextLastBlock = lastBlock->textPlainText().mid(
            lastBlock->indentedString().length() + lastBlock->blockDelimiter().length()
            + lastBlockSelectionEnd + lineBreaksLength);
    //    qDebug() << "savedTextLastBlock: " << savedTextLastBlock;

    QString savedPressedCharString = QKeySequence(savedPressedChar).toString();
    savedPressedCharString =
            isPressedCharLower ? savedPressedCharString.toLower() : savedPressedCharString;

    QString newFirstBlockPlainText =
            savedTextFirstBlock + savedPressedCharString + savedTextLastBlock;

    //    qDebug() << "newFirstBlockPlainText: " << newFirstBlockPlainText;
    updateBlockText(firstBlock, newFirstBlockPlainText, firstBlock->lineStartPos(),
                    firstBlock->lineEndPos());
    QModelIndex firstBlockIndex = this->index(selectedBlockIndexes[0]);
    emit dataChanged(firstBlockIndex, firstBlockIndex, {});

    //    qDebug() << "In Edit blocks";
    //    qDebug() << "firstIndex: " << selectedBlockIndexes[0];
    //    qDebug() << "lastIndex: " << selectedBlockIndexes[selectedBlockIndexes.length() - 1];

    updateSourceTextBetweenLines(selectedBlockIndexes[1],
                                 selectedBlockIndexes[selectedBlockIndexes.length() - 1], "", true,
                                 0, ActionType::Remove, OneCharOperation::NoOneCharOperation, false,
                                 firstBlockSelectionStart, lastBlockSelectionEnd);

    updateSourceTextBetweenLines(firstBlock->lineStartPos(), firstBlock->lineEndPos(),
                                 newFirstBlockPlainText, true, 0, ActionType::Modify,
                                 OneCharOperation::NoOneCharOperation, true);

    // Find the lowest indent level in selected blocks
    QList<int> allIndentLevels = {};
    for (int i = lastBlockIndex; i < m_blockList.length(); i++) {
        allIndentLevels.push_back(m_blockList[i]->indentLevel());
    }
    unsigned int minIndentLevel = *std::min_element(allIndentLevels.begin(), allIndentLevels.end());

    beginRemoveRows(QModelIndex(), selectedBlockIndexes[1],
                    selectedBlockIndexes[selectedBlockIndexes.length() - 1]);
    //    qDebug() << "start remove: " << selectedBlockIndexes[1];
    //    qDebug() << "end remove: " << selectedBlockIndexes[selectedBlockIndexes.length() - 1];
    for (int i = selectedBlockIndexes[1];
         i <= selectedBlockIndexes[selectedBlockIndexes.length() - 1]; i++) {
        QSharedPointer<BlockInfo> blockToRemove = m_blockList[i];
        for (auto &child : blockToRemove->children()) {
            if (child->parent() != nullptr)
                child->parent()->removeChild(child);
            //            child->setParent(nullptr);
        }
        if (blockToRemove->parent() != nullptr)
            blockToRemove->parent()->removeChild(blockToRemove);
        //        blockToRemove->deleteLater();
    }
    m_blockList.remove(selectedBlockIndexes[1], selectedBlockIndexes.length() - 1);
    endRemoveRows();

    // Redetermine child parent relationship for all blocks with indent level higher then the lowest
    // indent level found
    for (int i = selectedBlockIndexes[0] + 1; i < m_blockList.length(); i++) {
        QSharedPointer<BlockInfo> blockInfo = m_blockList[i];

        // We stop if we reach a block with an indent level equal/smaller than minIndentLevel
        if (blockInfo->indentLevel() <= minIndentLevel)
            break;

        if (blockInfo->indentLevel() > minIndentLevel) {
            determineBlockIndentAndParentChildRelationship(blockInfo, i - 1);
            QModelIndex modelIdxOfBlockIndentChanged = this->index(i);
            emit dataChanged(modelIdxOfBlockIndentChanged, modelIdxOfBlockIndentChanged, {});
        }
    }

    //    qDebug() << "selectedBlockIndexes: " << selectedBlockIndexes;
    //    qDebug() << "delta: " << -(selectedBlockIndexes.length() - 1);
    updateBlocksLinePositions(selectedBlockIndexes[1], -(selectedBlockIndexes.length() - 1));

    emit textChangeFinished();
}

void BlockModel::undo()
{
    if (!m_undoStack.isEmpty()) {
        emit aboutToChangeText();
        //        qDebug() << "m_undoStack.length: " << m_undoStack.length();
        CompoundAction &lastCompoundAction = m_undoStack.last();
        m_redoStack.push_back(lastCompoundAction);
        SingleAction actionWithSelectionToRestore;
        bool shouldRestoreSelection = false;
        for (auto &singleAction : lastCompoundAction.actions) {
            if (singleAction.actionType == ActionType::Modify) {
                qDebug() << "In undo Modify";
                unsigned int modifiedBlockIndex = singleAction.blockStartIndex;
                if (modifiedBlockIndex > m_blockList.length() - 1) {
                    continue;
                }
                qDebug() << "modifiedBlockIndex: " << modifiedBlockIndex;
                qDebug() << "m_blockList length: " << m_blockList.length();
                QSharedPointer<BlockInfo> blockInfo = m_blockList[modifiedBlockIndex];
                updateBlockUsingPlainText(blockInfo, blockInfo->lineStartPos(),
                                          singleAction.oldPlainText);
                updateSourceTextBetweenLines(singleAction.blockStartIndex,
                                             singleAction.blockEndIndex, singleAction.oldPlainText,
                                             false);
                emit restoreCursorPosition(singleAction.lastCursorPosition);
                //                qDebug() << "Here 1";
                QModelIndex modelIdx = this->index(modifiedBlockIndex);
                //                qDebug() << "Here 2";
                emit dataChanged(modelIdx, modelIdx, {});
                //                qDebug() << "Here 3";
            } else if (singleAction.actionType == ActionType::Remove) {
                qDebug() << "In undo Remove";
                //                qDebug() << "blockStartIndex: " << singleAction.blockStartIndex;
                //                qDebug() << "blockEndIndex: " << singleAction.blockEndIndex;
                //                qDebug() << "oldPlainText: " << singleAction.oldPlainText;
                updateSourceTextBetweenLines(singleAction.blockStartIndex,
                                             singleAction.blockEndIndex, singleAction.oldPlainText,
                                             false, 0, ActionType::Insert,
                                             OneCharOperation::NoOneCharOperation, false);
                QStringList lines =
                        singleAction.oldPlainText.split("\n"); // TODO: consider optimizing this
                //                qDebug() << "lines: " << lines;
                beginInsertRows(QModelIndex(), singleAction.blockStartIndex,
                                singleAction.blockEndIndex);
                int index = 0;
                for (auto &line : lines) {
                    int blockIndex = singleAction.blockStartIndex + index;
                    //                    qDebug() << "blockIndex: " << blockIndex;
                    //                    BlockInfo *blockInfo = new BlockInfo(this);
                    QSharedPointer<BlockInfo> blockInfo =
                            QSharedPointer<BlockInfo>(new BlockInfo());
                    m_blockList.insert(blockIndex, blockInfo);
                    updateBlockUsingPlainText(blockInfo, blockIndex, line);
                    //                    qDebug() << "blockStartLinePos:" <<
                    //                    blockInfo->lineStartPos(); qDebug() << "blockEndLinePos:"
                    //                    << blockInfo->lineEndPos();
                    index++;
                }
                endInsertRows();
                updateBlocksLinePositions(singleAction.blockEndIndex + 1, lines.length());

                actionWithSelectionToRestore.blockStartIndex = singleAction.blockStartIndex;
                actionWithSelectionToRestore.blockEndIndex = singleAction.blockEndIndex;
                actionWithSelectionToRestore.firstBlockSelectionStart =
                        singleAction.firstBlockSelectionStart;
                actionWithSelectionToRestore.lastBlockSelectionEnd =
                        singleAction.lastBlockSelectionEnd;
                shouldRestoreSelection = true;
                QSharedPointer<BlockInfo> firstBlockInfo =
                        m_blockList[singleAction.blockStartIndex - 1];
                for (int i = singleAction.blockEndIndex + 1; i < m_blockList.length(); i++) {
                    QSharedPointer<BlockInfo> blockInfo = m_blockList[i];
                    if (blockInfo->indentedString().length() == 0
                        || blockInfo->indentedString().length()
                                <= firstBlockInfo->indentedString().length()) {
                        break;
                    }
                    determineBlockIndentAndParentChildRelationship(blockInfo, i - 1);
                    QModelIndex modelIdx = this->index(i);
                    emit dataChanged(modelIdx, modelIdx, {});
                }
            } else if (singleAction.actionType == ActionType::Insert) {
                qDebug() << "In undo Insert";
                //                int blockIndex = singleAction.blockStartIndex;
                //                QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];
                //                beginRemoveRows(QModelIndex(), blockIndex, blockIndex);
                //                updateSourceTextBetweenLines(blockInfo->lineStartPos(),
                //                blockInfo->lineEndPos(), "",
                //                                             false, 0, ActionType::Remove,
                //                                             OneCharOperation::NoOneCharOperation,
                //                                             false);
                //                for (auto &child : blockInfo->children()) {
                //                    if (child->parent() != nullptr)
                //                        child->parent()->removeChild(child);
                //                    //                    child->setParent(nullptr);
                //                    determineBlockIndentAndParentChildRelationship(child,
                //                    blockIndex - 1);
                //                }
                //                m_blockList.removeAt(blockIndex);
                //                //                blockInfo->deleteLater();
                //                endRemoveRows();
                //                updateBlocksLinePositions(blockIndex, -1);
                //                emit blockToFocusOnChanged(blockIndex - 1);

                qDebug() << "blockStartIndex: " << singleAction.blockStartIndex;
                qDebug() << "blockEndIndex: " << singleAction.blockEndIndex;

                updateSourceTextBetweenLines(
                        singleAction.blockStartIndex, singleAction.blockEndIndex, "", false, 0,
                        ActionType::Remove, OneCharOperation::NoOneCharOperation, false);
                int blockEndIndex =
                        singleAction.blockEndIndex; // singleAction.blockEndIndex >=
                                                    // m_blockList.length() ? m_blockList.length() -
                                                    // 1 : singleAction.blockEndIndex;
                for (unsigned int blockIndex = singleAction.blockStartIndex;
                     blockIndex <= blockEndIndex; blockIndex++) {
                    QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];
                    for (auto &child : blockInfo->children()) {
                        if (child->parent() != nullptr)
                            child->parent()->removeChild(child);
                        //                        child->setParent(nullptr);
                        determineBlockIndentAndParentChildRelationship(child, blockIndex - 1);
                    }
                    //                    blockInfo->deleteLater();
                }
                beginRemoveRows(QModelIndex(), singleAction.blockStartIndex, blockEndIndex);
                m_blockList.remove(singleAction.blockStartIndex,
                                   blockEndIndex - singleAction.blockStartIndex + 1);
                endRemoveRows();
                emit blockToFocusOnChanged(singleAction.blockStartIndex - 1);
                updateBlocksLinePositions(singleAction.blockStartIndex,
                                          -(blockEndIndex - singleAction.blockStartIndex + 1));
                // We need to run through all the next blocks and reevaluate their parent-child
                // relationship
                QSharedPointer<BlockInfo> firstBlockInfo =
                        m_blockList[singleAction.blockStartIndex - 1];
                for (int i = singleAction.blockStartIndex; i < m_blockList.length(); i++) {
                    QSharedPointer<BlockInfo> blockInfo = m_blockList[i];
                    if (blockInfo->indentedString().length() == 0
                        || blockInfo->indentedString().length()
                                <= firstBlockInfo->indentedString().length()) {
                        break;
                    }
                    determineBlockIndentAndParentChildRelationship(blockInfo, i - 1);
                    QModelIndex modelIdx = this->index(i);
                    emit dataChanged(modelIdx, modelIdx, {});
                }
            }
        }

        //        qDebug() << "Here 4";
        m_undoStack.removeLast();
        //        qDebug() << "Here 5";
        emit textChangeFinished();
        if (shouldRestoreSelection) {
            emit restoreSelection(actionWithSelectionToRestore.blockStartIndex,
                                  actionWithSelectionToRestore.blockEndIndex,
                                  actionWithSelectionToRestore.firstBlockSelectionStart,
                                  actionWithSelectionToRestore.lastBlockSelectionEnd);
        }
    }
}

void BlockModel::redo()
{
    if (!m_redoStack.isEmpty()) {
        emit aboutToChangeText();
        //        qDebug() << "m_redoStack.length: " << m_redoStack.length();
        CompoundAction &lastCompoundAction = m_redoStack.last();
        m_undoStack.push_back(lastCompoundAction);
        //        SingleAction actionWithSelectionToRestore;
        //        bool shouldRestoreSelection = false;
        // We need to iterate in reverse during redo
        //        for (auto it = lastCompoundAction.actions.rbegin(); it !=
        //        lastCompoundAction.actions.rend(); ++it) {
        //            auto &singleAction = *it;
        for (int actionIndex = lastCompoundAction.actions.size() - 1; actionIndex >= 0;
             --actionIndex) {
            auto &singleAction = lastCompoundAction.actions[actionIndex];
            if (singleAction.actionType == ActionType::Modify) {
                qDebug() << "In redo Modify";
                unsigned int modifiedBlockIndex = singleAction.blockStartIndex;
                if (modifiedBlockIndex > m_blockList.length() - 1) {
                    continue;
                }
                QSharedPointer<BlockInfo> blockInfo = m_blockList[modifiedBlockIndex];
                updateBlockUsingPlainText(blockInfo, blockInfo->lineStartPos(),
                                          singleAction.newPlainText);
                updateSourceTextBetweenLines(singleAction.blockStartIndex,
                                             singleAction.blockEndIndex, singleAction.newPlainText,
                                             false);
                emit restoreCursorPosition(singleAction.lastCursorPosition);
                QModelIndex modelIdx = this->index(modifiedBlockIndex);
                emit dataChanged(modelIdx, modelIdx, {});
            } else if (singleAction.actionType == ActionType::Remove) {
                qDebug() << "In redo Remove";
                updateSourceTextBetweenLines(
                        singleAction.blockStartIndex, singleAction.blockEndIndex, "", false, 0,
                        ActionType::Remove, OneCharOperation::NoOneCharOperation, false);
                for (unsigned int blockIndex = singleAction.blockStartIndex;
                     blockIndex <= singleAction.blockEndIndex; blockIndex++) {
                    QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];
                    for (auto &child : blockInfo->children()) {
                        if (child->parent() != nullptr)
                            child->parent()->removeChild(child);
                        //                        child->setParent(nullptr);
                        determineBlockIndentAndParentChildRelationship(child, blockIndex - 1);
                    }
                    //                    blockInfo->deleteLater();
                }
                beginRemoveRows(QModelIndex(), singleAction.blockStartIndex,
                                singleAction.blockEndIndex);
                m_blockList.remove(singleAction.blockStartIndex,
                                   singleAction.blockEndIndex - singleAction.blockStartIndex + 1);
                endRemoveRows();
                emit blockToFocusOnChanged(singleAction.blockStartIndex - 1);
                updateBlocksLinePositions(
                        singleAction.blockStartIndex,
                        -(singleAction.blockEndIndex - singleAction.blockStartIndex + 1));
                // We need to run through all the next blocks and reevaluate their parent-child
                // relationship
                QSharedPointer<BlockInfo> firstBlockInfo =
                        m_blockList[singleAction.blockStartIndex - 1];
                for (int i = singleAction.blockStartIndex; i < m_blockList.length(); i++) {
                    QSharedPointer<BlockInfo> blockInfo = m_blockList[i];
                    if (blockInfo->indentedString().length() == 0
                        || blockInfo->indentedString().length()
                                <= firstBlockInfo->indentedString().length()) {
                        break;
                    }
                    determineBlockIndentAndParentChildRelationship(blockInfo, i - 1);
                    QModelIndex modelIdx = this->index(i);
                    emit dataChanged(modelIdx, modelIdx, {});
                }
            } else if (singleAction.actionType == ActionType::Insert) {
                qDebug() << "In redo Insert";
                //                int blockIndex = singleAction.blockStartIndex;
                //                updateSourceTextBetweenLines(singleAction.blockStartIndex,
                //                                             singleAction.blockEndIndex,
                //                                             singleAction.newPlainText, false, 0,
                //                                             ActionType::Insert,
                //                                             OneCharOperation::NoOneCharOperation,
                //                                             false);
                //                beginInsertRows(QModelIndex(), blockIndex, blockIndex);
                //                //                BlockInfo *blockInfo = new BlockInfo(this);
                //                QSharedPointer<BlockInfo> blockInfo =
                //                QSharedPointer<BlockInfo>(new BlockInfo());
                //                m_blockList.insert(blockIndex, blockInfo);
                //                updateBlockUsingPlainText(blockInfo, blockIndex,
                //                singleAction.newPlainText); endInsertRows(); emit
                //                blockToFocusOnChanged(blockIndex);
                //                updateBlocksLinePositions(blockIndex + 1, 1);

                updateSourceTextBetweenLines(singleAction.blockStartIndex,
                                             singleAction.blockEndIndex, singleAction.newPlainText,
                                             false, 0, ActionType::Insert,
                                             OneCharOperation::NoOneCharOperation, false);
                QStringList lines =
                        singleAction.newPlainText.split("\n"); // TODO: consider optimizing this
                //                                qDebug() << "lines: " << lines;
                beginInsertRows(QModelIndex(), singleAction.blockStartIndex,
                                singleAction.blockEndIndex);
                int index = 0;
                for (auto &line : lines) {
                    int blockIndex = singleAction.blockStartIndex + index;
                    //                    qDebug() << "blockIndex: " << blockIndex;
                    //                    BlockInfo *blockInfo = new BlockInfo(this);
                    QSharedPointer<BlockInfo> blockInfo =
                            QSharedPointer<BlockInfo>(new BlockInfo());
                    m_blockList.insert(blockIndex, blockInfo);
                    updateBlockUsingPlainText(blockInfo, blockIndex, line);
                    //                    qDebug() << "blockStartLinePos:" <<
                    //                    blockInfo->lineStartPos(); qDebug() << "blockEndLinePos:"
                    //                    << blockInfo->lineEndPos();
                    index++;
                }
                endInsertRows();
                updateBlocksLinePositions(singleAction.blockEndIndex + 1, lines.length());
                //                actionWithSelectionToRestore.blockStartIndex =
                //                singleAction.blockStartIndex;
                //                actionWithSelectionToRestore.blockEndIndex =
                //                singleAction.blockEndIndex;
                //                actionWithSelectionToRestore.firstBlockSelectionStart =
                //                        singleAction.firstBlockSelectionStart;
                //                actionWithSelectionToRestore.lastBlockSelectionEnd =
                //                        singleAction.lastBlockSelectionEnd;
                //                shouldRestoreSelection = true;
                QSharedPointer<BlockInfo> firstBlockInfo =
                        m_blockList[singleAction.blockStartIndex - 1];
                for (int i = singleAction.blockEndIndex + 1; i < m_blockList.length(); i++) {
                    QSharedPointer<BlockInfo> blockInfo = m_blockList[i];
                    if (blockInfo->indentedString().length() == 0
                        || blockInfo->indentedString().length()
                                <= firstBlockInfo->indentedString().length()) {
                        break;
                    }
                    determineBlockIndentAndParentChildRelationship(blockInfo, i - 1);
                    QModelIndex modelIdx = this->index(i);
                    emit dataChanged(modelIdx, modelIdx, {});
                }
                emit blockToFocusOnChanged(singleAction.blockEndIndex);
                QSharedPointer<BlockInfo> lastBlock = m_blockList[singleAction.blockEndIndex];
                int indentAndDelimiterLengthLastBlock =
                        lastBlock->indentedString().length() + lastBlock->blockDelimiter().length();
                emit restoreCursorPosition(
                        lastBlock->textPlainText().replace("<br />", "\n").length()
                        - indentAndDelimiterLengthLastBlock);
            }
        }

        m_redoStack.removeLast();
        emit textChangeFinished();
        //        if (shouldRestoreSelection) {
        //            emit restoreSelection(actionWithSelectionToRestore.blockStartIndex,
        //            actionWithSelectionToRestore.blockEndIndex,
        //            actionWithSelectionToRestore.firstBlockSelectionStart,
        //            actionWithSelectionToRestore.lastBlockSelectionEnd);
        //        }
    }
}

void BlockModel::setVerticalScrollBarPosition(double scrollBarPosition, int itemIndexInView)
{
    //    qDebug() << "scrollBarPosition: " << scrollBarPosition;
    //    qDebug() << "itemIndexInView: " << itemIndexInView;
    m_verticalScrollBarPosition = scrollBarPosition;
    m_itemIndexInView = itemIndexInView;
    emit verticalScrollBarPositionChanged(m_verticalScrollBarPosition, m_itemIndexInView);
}

int BlockModel::getBlockTextLengthWithoutIndentAndDelimiter(int blockIndex)
{
    if (blockIndex < 0 || blockIndex > m_blockList.length() - 1)
        return 0;

    QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];
    QString plainText = blockInfo->textPlainText();
    plainText = plainText.mid(blockInfo->indentedString().length()
                              + blockInfo->blockDelimiter().length());
    return plainText.length();
}

void BlockModel::copy(QList<int> selectedBlockIndexes, int firstBlockSelectionStartPos,
                      int lastBlockSelectionEndPos)
{
    std::sort(selectedBlockIndexes.begin(), selectedBlockIndexes.end());
    int firstSelectedBlockIndex = selectedBlockIndexes[0];
    int lastSelectedBlockIndex = selectedBlockIndexes[selectedBlockIndexes.length() - 1];
    QString copiedText = "";
    QSharedPointer<BlockInfo> firstBlock = m_blockList[selectedBlockIndexes[0]];
    int lineBreakCount = firstBlock->textPlainText().count("<br />");
    int lineBreaksLength = (lineBreakCount * QStringLiteral("<br />").length()) - lineBreakCount;
    int indentAndDelimiterLength =
            firstBlock->indentedString().length() + firstBlock->blockDelimiter().length();
    int indentAndDelimiterAndLineBreaksLength = indentAndDelimiterLength + lineBreaksLength;

    if (firstBlockSelectionStartPos == 0
        && (selectedBlockIndexes.length() > 1
            || lastBlockSelectionEndPos + indentAndDelimiterAndLineBreaksLength
                    == firstBlock->textPlainText().length())) {
        copiedText += firstBlock->textPlainText();
    } else if (firstSelectedBlockIndex == lastSelectedBlockIndex) {
        // We need to replace the <br> with \n since in the QML editor lines breaks are one
        // (unicode) character

        copiedText += firstBlock->textPlainText()
                              .replace("<br />", "\n")
                              .mid(indentAndDelimiterLength + firstBlockSelectionStartPos,
                                   lastBlockSelectionEndPos - firstBlockSelectionStartPos);
        copiedText.replace("\n", "<br />");
    } else {
        copiedText += firstBlock->textPlainText()
                              .replace("<br />", "\n")
                              .mid(indentAndDelimiterLength + firstBlockSelectionStartPos);
        copiedText.replace("\n", "<br />");
    }

    if (selectedBlockIndexes.length() > 2) {
        int startLinePos = selectedBlockIndexes[1];
        int endLinePos = selectedBlockIndexes[selectedBlockIndexes.length() - 2];
        QTextBlock startBlock = m_sourceDocument.findBlockByLineNumber(startLinePos);
        QTextBlock endBlock = m_sourceDocument.findBlockByLineNumber(endLinePos);
        QTextCursor cursor(startBlock);
        cursor.setPosition(startBlock.position(), QTextCursor::MoveAnchor);
        cursor.setPosition(endBlock.position() + endBlock.length() - 1, QTextCursor::KeepAnchor);
        copiedText += "\n" + cursor.selectedText().replace("\u2029", "\n");
    }

    if (selectedBlockIndexes.length() > 1) {
        QSharedPointer<BlockInfo> lastBlock =
                m_blockList[selectedBlockIndexes[selectedBlockIndexes.length() - 1]];
        QString tempLastBlockText = lastBlock->textPlainText();
        tempLastBlockText.replace("<br />", "\n");
        int indentAndDelimiterLastBlock =
                lastBlock->indentedString().length() + lastBlock->blockDelimiter().length();
        tempLastBlockText.truncate(lastBlockSelectionEndPos + indentAndDelimiterLastBlock);
        copiedText += "\n" + tempLastBlockText;
    }

    copiedText.replace("<br />", "\n");

    m_clipboard->setText(copiedText);
}

void BlockModel::pasteMarkdown(const QString &markdown, QList<int> &selectedBlockIndexes,
                               int firstBlockSelectionStartPos, int lastBlockSelectionEndPos)
{
    emit aboutToChangeText();
    int firstSelectedBlockIndex = selectedBlockIndexes[0];
    int lastSelectedBlockIndex = selectedBlockIndexes[selectedBlockIndexes.length() - 1];
    int numberOfSelectedBlocks = lastSelectedBlockIndex - firstSelectedBlockIndex + 1;
    QStringList lines = markdown.split("\n");
    int index = 0;
    BlockInfo::BlockType previousBlockType = BlockInfo::BlockType::RegularText;
    QStringList linesToInsert = {};
    int numberOfActualInsertions = 0;
    int firstActualInsertionIndex = 0;

    // Saving last block's text, if exist
    QSharedPointer<BlockInfo> lastSelectedBlock = m_blockList[lastSelectedBlockIndex];
    int indentAndDelimiterLength = lastSelectedBlock->indentedString().length()
            + lastSelectedBlock->blockDelimiter().length();
    QString savedTextForLastBlock = lastSelectedBlock->textPlainText();
    savedTextForLastBlock.replace("<br />", "\n");
    ;
    savedTextForLastBlock =
            savedTextForLastBlock.mid(indentAndDelimiterLength + lastBlockSelectionEndPos);
    savedTextForLastBlock.replace("\n", "<br />");

    //        if (numberOfSelectedBlocks < lines.length()) {
    //            int firstInsertIndex = lastSelectedBlockIndex + 1;
    //            int lastInsertIndex = lastSelectedBlockIndex + (lines.length() -
    //            numberOfSelectedBlocks); beginInsertRows(QModelIndex(), firstInsertIndex,
    //            lastInsertIndex);
    //        }

    for (auto &line : lines) {
        //            qDebug() << "line: " << line;
        BlockInfo::BlockType currentLineBlockType = BlockInfo::determineBlockTypeHelper(line);
        if (!line.isEmpty() && currentLineBlockType == BlockInfo::BlockType::RegularText
            && (previousBlockType == BlockInfo::BlockType::Quote
                || previousBlockType == BlockInfo::BlockType::Todo
                || previousBlockType == BlockInfo::BlockType::NumberedListItem
                || previousBlockType == BlockInfo::BlockType::BulletListItem)) {
            // If we get a consecutive line break with some regularText
            // And the previous block is of a certain type (quote, todo etc...)
            // We'll append the current line's text to the previous block
            // And continue the loop
            //                qDebug() << "In appending to previoud block...";
            //                qDebug() << "Previous block index: " << firstSelectedBlockIndex +
            //                index - 1; qDebug() << "previousBlockType: " << previousBlockType;
            //                qDebug() << "newText: " << newText;
            //                qDebug() << " 3.1: " << previousBlock->lineStartPos();
            //                qDebug() << " 3.2: " << previousBlock->lineEndPos();
            QSharedPointer<BlockInfo> previousBlock =
                    m_blockList[firstSelectedBlockIndex + index - 1];
            QString newText = previousBlock->textPlainText() + "<br />" + line;
            if (index < numberOfSelectedBlocks) {
                //                    qDebug() << "Append to previous existing block...";
                updateSourceTextBetweenLines(previousBlock->lineStartPos(),
                                             previousBlock->lineEndPos(), newText, true,
                                             lastBlockSelectionEndPos, ActionType::Modify,
                                             OneCharOperation::NoOneCharOperation, true);
                updateBlockUsingPlainText(previousBlock, firstSelectedBlockIndex + index - 1,
                                          newText);
                QModelIndex modelIdx = this->index(firstSelectedBlockIndex + index - 1);
                emit dataChanged(modelIdx, modelIdx, {});
            } else if (linesToInsert.length() > 0) {
                //                    qDebug() << "Append to previous inserted block...";
                linesToInsert[linesToInsert.length() - 1] =
                        linesToInsert[linesToInsert.length() - 1] + "<br />" + line;
                updateBlockUsingPlainText(previousBlock, firstSelectedBlockIndex + index - 1,
                                          newText);
            }
            continue;
        }
        if (index < numberOfSelectedBlocks) {
            // We modify an existing block
            // If the entire text of the first block is selected, if it's not regularText,
            // We'll turn it into regularText and redetermine the block type based on the current
            // line
            QSharedPointer<BlockInfo> currentBlock = m_blockList[firstSelectedBlockIndex + index];
            // Since firstBlockSelectionStartPos and lastBlockSelectionEndPos are coming from QML
            // They aren't the same position as the ones in the source text (with <br />)
            // And since we need to get text from the middle of the block, we can't just add
            // indentAndDelimiterAndLineBreaksLength to make it work. We'll need to add only the end
            // breaks length till the first block selection start pos
            QString newText;
            int indentAndDelimiterLength = currentBlock->indentedString().length()
                    + currentBlock->blockDelimiter().length();
            //                bool isCurrentBlockEndBlock = firstSelectedBlockIndex + index ==
            //                lastSelectedBlockIndex; qDebug() << "isCurrentBlockEndBlock: " <<
            //                isCurrentBlockEndBlock; qDebug() << "firstSelectedBlockIndex: " <<
            //                firstSelectedBlockIndex; qDebug() << "index: " << index; qDebug() <<
            //                "lastSelectedBlockIndex: " << lastSelectedBlockIndex;
            if (index == 0) { // || isCurrentBlockEndBlock) {
                newText = currentBlock->textPlainText();
                newText.replace("<br />", "\n");

                //                    if (isCurrentBlockEndBlock) {
                //                        qDebug() << "in isCurrentBlockEndBlock";
                //                        savedTextForLastBlock =
                //                        newText.mid(indentAndDelimiterLength +
                //                        lastBlockSelectionEndPos);
                //                        savedTextForLastBlock.replace("\n", "<br />");
                //                    }
            }
            if (index == 0) {
                newText.truncate(indentAndDelimiterLength + firstBlockSelectionStartPos);
                newText += line;
                newText.replace("\n", "<br />");
            } else {
                newText = line;
            }
            //                qDebug() << "Modify existing block...";
            updateSourceTextBetweenLines(currentBlock->lineStartPos(), currentBlock->lineEndPos(),
                                         newText, true, lastBlockSelectionEndPos,
                                         ActionType::Modify, OneCharOperation::NoOneCharOperation,
                                         index != 0, firstBlockSelectionStartPos,
                                         lastBlockSelectionEndPos);
            updateBlockUsingPlainText(currentBlock, firstSelectedBlockIndex + index, newText);
            QModelIndex modelIdx = this->index(firstSelectedBlockIndex + index);
            emit dataChanged(modelIdx, modelIdx, {});

        } else {
            //                qDebug() << "Inserting block...";
            // We need to create a new block and insert it
            //                qDebug() << "Creating a new block...";
            int newBlockIndex = firstSelectedBlockIndex + index;
            //                qDebug() << "newBlockIndex: " << newBlockIndex;
            if (numberOfActualInsertions == 0) {
                firstActualInsertionIndex = newBlockIndex;
            }
            //                updateSourceTextBetweenLines(newBlockIndex,
            //                                             newBlockIndex, line, true, 0,
            //                                             ActionType::Insert,
            //                                             OneCharOperation::NoOneCharOperation,
            //                                             true);
            QSharedPointer<BlockInfo> newBlockInfo = QSharedPointer<BlockInfo>(new BlockInfo());
            updateBlockUsingPlainText(newBlockInfo, firstSelectedBlockIndex + index, line);
            m_blockList.insert(newBlockIndex, newBlockInfo);
            linesToInsert.push_back(line);
            numberOfActualInsertions++;
        }

        //            qDebug() << "index: " << index;
        //            qDebug() << "currentLineBlockType: " << currentLineBlockType;

        previousBlockType = currentLineBlockType;
        index++;
    } // End of loop

    // index now = modified lines + actual inserted lines

    if (numberOfSelectedBlocks < index) {
        //            qDebug() << "firstActualInsertionIndex: " << firstActualInsertionIndex;
        //            qDebug() << "firstActualInsertionIndex + numberOfActualInsertions - 1: " <<
        //            firstActualInsertionIndex + numberOfActualInsertions - 1;
        beginInsertRows(QModelIndex(), firstActualInsertionIndex,
                        firstActualInsertionIndex + numberOfActualInsertions - 1);
        //            int firstInsertIndex = lastSelectedBlockIndex + 1;
        //            int lastInsertIndex = firstSelectedBlockIndex + index - 1;
        //            QString linesToInsert = lines.mid(numberOfSelectedBlocks).join("\n");
        //            qDebug() << "linesToInsert: " << linesToInsert;
        //            qDebug() << "firstInsertIndex: " << firstInsertIndex;
        //            qDebug() << "lastInsertIndex: " << lastInsertIndex;
        updateSourceTextBetweenLines(firstActualInsertionIndex,
                                     firstActualInsertionIndex + numberOfActualInsertions - 1,
                                     linesToInsert.join("\n"), true, 0, ActionType::Insert,
                                     OneCharOperation::NoOneCharOperation, true);
        endInsertRows();
        //            int linesDelta = lines.length() - numberOfSelectedBlocks;
        updateBlocksLinePositions(firstSelectedBlockIndex + index, numberOfActualInsertions);
    }

    //        qDebug() << "savedTextForLastBlock: " << savedTextForLastBlock;
    int lastBlockIndex = firstSelectedBlockIndex + index - 1;
    QSharedPointer<BlockInfo> lastBlock = m_blockList[lastBlockIndex];
    int indentAndDelimiterLengthLastBlock =
            lastBlock->indentedString().length() + lastBlock->blockDelimiter().length();
    int cursorPositionBeforeAddingSavedTextQml =
            lastBlock->textPlainText().replace("<br />", "\n").length()
            - indentAndDelimiterLengthLastBlock;
    if (savedTextForLastBlock.length() > 0) {
        //            qDebug() << "Appending saved text to last block";
        // If exist, we add the saved text to the last block
        QString newText = lastBlock->textPlainText() + savedTextForLastBlock;
        //            qDebug() << "newText: " << newText;
        //            qDebug() << " 1: " << lastBlock->lineEndPos();
        //            bool shouldSaveUndo = lastSelectedBlockIndex != firstSelectedBlockIndex;
        //            qDebug() << "shouldSaveUndo: " << shouldSaveUndo;
        updateSourceTextBetweenLines(lastBlock->lineStartPos(), lastBlock->lineEndPos(), newText,
                                     true, lastBlockSelectionEndPos, ActionType::Modify,
                                     OneCharOperation::NoOneCharOperation, true);
        updateBlockUsingPlainText(lastBlock, lastBlockIndex, newText);
        QModelIndex modelIdx = this->index(lastBlockIndex);
        emit dataChanged(modelIdx, modelIdx, {});
    }

    qDebug() << "numberOfSelectedBlocks: " << numberOfSelectedBlocks;
    qDebug() << "lines length: " << lines.length();
    qDebug() << "index: " << index;
    if (numberOfSelectedBlocks > index) {
        //            qDebug() << "Removing blocks...";
        // We need to remove the remaining blocks
        int startRemoveIndex = firstSelectedBlockIndex + index;
        int endRemoveIndex = lastSelectedBlockIndex;
        QSharedPointer<BlockInfo> startBlock = m_blockList[startRemoveIndex];
        QSharedPointer<BlockInfo> endBlock = m_blockList[endRemoveIndex];
        updateSourceTextBetweenLines(startBlock->lineStartPos(), endBlock->lineEndPos(), "", true,
                                     0, ActionType::Remove, OneCharOperation::NoOneCharOperation,
                                     true);
        beginRemoveRows(QModelIndex(), startRemoveIndex, endRemoveIndex);
        for (int j = startRemoveIndex; j <= endRemoveIndex; j++) {
            QSharedPointer<BlockInfo> blockToRemove = m_blockList[j];
            for (auto &child : blockToRemove->children()) {
                if (child->parent() != nullptr)
                    child->parent()->removeChild(child);
            }
            if (blockToRemove->parent() != nullptr)
                blockToRemove->parent()->removeChild(blockToRemove);
        }
        m_blockList.remove(startRemoveIndex, endRemoveIndex - startRemoveIndex + 1);
        endRemoveRows();
        updateBlocksLinePositions(startRemoveIndex, -(endRemoveIndex - startRemoveIndex + 1));
    }

    emit blockToFocusOnChanged(firstSelectedBlockIndex + index - 1);
    emit restoreCursorPosition(cursorPositionBeforeAddingSavedTextQml);
    emit textChangeFinished();
}

void BlockModel::paste(QList<int> selectedBlockIndexes, int firstBlockSelectionStartPos,
                       int lastBlockSelectionEndPos)
{
    std::sort(selectedBlockIndexes.begin(), selectedBlockIndexes.end());
    const QMimeData *clipboardMimeData = m_clipboard->mimeData();

    if (clipboardMimeData->hasHtml()) {
        html2md::Options options;
        options.splitLines = false;
        std::string html = clipboardMimeData->html().toStdString();
        html2md::Converter c(html, &options);
        QString markdown = QString::fromStdString(c.convert());
        //        qDebug() << "markdown: " << markdown;
        pasteMarkdown(markdown, selectedBlockIndexes, firstBlockSelectionStartPos,
                      lastBlockSelectionEndPos);
    } else if (clipboardMimeData->hasImage()) {

    } else if (clipboardMimeData->hasText()) {
        pasteMarkdown(clipboardMimeData->text(), selectedBlockIndexes, firstBlockSelectionStartPos,
                      lastBlockSelectionEndPos);
    }
}

QString BlockModel::getSourceText()
{
    return m_sourceDocument.toPlainText();
}

void BlockModel::setSourceText(const QString &text)
{
    m_sourceDocument.setPlainText(text);
    emit textChangeFinished();
}

void BlockModel::onSearchEditTextChanged(const QString &keyword) {
    m_searchResultBlockIndexes.clear();
    m_searchKeyword = keyword;
}

void BlockModel::clearSearch()
{
    m_searchResultBlockIndexes.clear();
    m_searchKeyword = "";
}

void BlockModel::setTheme(Theme::Value theme)
{
    m_currentTheme = theme;
}


bool isWithinMarkup(const QString& text, int position) {
    if (position < 0 || position >= text.size()) {
        return false; // Position out of string bounds
    }

    bool withinAsteriskMarkup = false;
    bool withinTildeMarkup = false;

    for (int i = 0; i <= position; ++i) {
        // Check for double asterisks
        if (i < text.size() - 1 && text[i] == '*' && text[i + 1] == '*') {
            withinAsteriskMarkup = !withinAsteriskMarkup; // Toggle state
            ++i; // Skip next asterisk
        }
        // Check for tilde
        else if (text[i] == '~') {
            withinTildeMarkup = !withinTildeMarkup; // Toggle state
        }
    }

    return withinAsteriskMarkup || withinTildeMarkup;
}

void BlockModel::checkToRenderMarkdown(const int blockIndex, int cursorPositionQML)
{
    QSharedPointer<BlockInfo> blockInfo = m_blockList[blockIndex];
    QString markdown = blockInfo->textPlainText();
    int indentAndDelimiterLength =
            blockInfo->indentedString().length() + blockInfo->blockDelimiter().length();
    int lineBreaksInQmlLength = markdown.replace("<br />", "\n").count("\n") * QStringLiteral("<br />").length();
    int cursorPositionPlainText = indentAndDelimiterLength + lineBreaksInQmlLength + cursorPositionQML;

    qDebug() << "blockInfo->textPlainText(): " << blockInfo->textPlainText();
    qDebug() << "cursorPositionPlainText: " << cursorPositionPlainText;

    if (isWithinMarkup(blockInfo->textPlainText(), cursorPositionPlainText)) {
        qDebug() << "In Markup";
        blockInfo->setTextHtml(blockInfo->textPlainText());
        QModelIndex modelIdx = this->index(blockIndex);
        emit dataChanged(modelIdx, modelIdx, {});
    } else {
        qDebug() << "Not in Markup";
        QString mark = blockInfo->textPlainText();
        updateBlockUsingPlainText(blockInfo, blockIndex, mark);
        QModelIndex modelIdx = this->index(blockIndex);
        emit dataChanged(modelIdx, modelIdx, {});
    }
}
