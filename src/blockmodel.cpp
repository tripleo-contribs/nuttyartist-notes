#include "blockmodel.h"

BlockModel::BlockModel(QObject *parent)
    : QAbstractListModel{parent},
      m_sourceDocument(this),
      m_blockList({}),
      m_htmlMetaDataStart(QStringLiteral("<html><head><meta name=\"qrichtext\" content=\"1\" /><meta charset=\"utf-8\" /></head><body>")),
      m_htmlMetaDataEnd(QStringLiteral("</body></html>")),
      m_tabLengthInSpaces(4),
      m_textLineHeightInPercentage(125),
      m_blockIndexToFocusOn(0)
{
    Q_UNUSED(parent);
}

int BlockModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_blockList.length();
}

QVariant BlockModel::data(const QModelIndex &index, int role) const
{
    if (index.isValid() && index.row() >= 0 && index.row() < m_blockList.length()) {
        BlockInfo * blockInfo = m_blockList[index.row()];

        switch((Role) role) {
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

QTextDocument* BlockModel::sourceDocument()
{
    return &m_sourceDocument;
}

void BlockModel::processOutput(const MD_CHAR* output, MD_SIZE size, void* userdata) {
    QByteArray* buffer = reinterpret_cast<QByteArray*>(userdata);
    buffer->append(output, size);
}

QString BlockModel::markdownToHtml(const QString &markdown) {
    QByteArray md = markdown.toUtf8();
    QByteArray html;

    unsigned parser_flags = MD_FLAG_STRIKETHROUGH | MD_FLAG_NOINDENTEDCODEBLOCKS;

    if(md_html((MD_CHAR*)md.data(), md.size(), &processOutput, (void*)&html, parser_flags, 0) == 0)
        return QString::fromUtf8(html);
    else
        return QString();  // or throw an exception
}

// TODO: this function should be in BlockInfo not here
unsigned int BlockModel::calculateTotalIndentLength(const QString &str, BlockInfo* blockInfo)
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
void BlockModel::updateBlockText(BlockInfo* blockInfo, const QString &plainText, unsigned int lineStartPos, unsigned int lineEndPos) {
    // Plain text
    blockInfo->setTextPlainText(plainText);

   // HTML
   // We clean the plain text string from:
   // 1. Indentation at the start of the string
   // 2. Delimeter
    QString htmlOutput = plainText.mid(blockInfo->indentedString().length() + blockInfo->blockDelimiter().length());

//    qDebug() << "BlockType (in updateBlockText): " << blockInfo->blockType();
//    qDebug() << "htmlOutput 1: " << htmlOutput;
    htmlOutput =  m_htmlMetaDataStart + (htmlOutput.length() < 2 ? htmlOutput : markdownToHtml(htmlOutput)) + m_htmlMetaDataEnd;
//    qDebug() << "htmlOutput 2: " << htmlOutput;
    if (blockInfo->blockType() != BlockInfo::BlockType::Heading) {
        htmlOutput.replace("<p>", QStringLiteral("<p style=\"line-height:%1%;\">").arg(m_textLineHeightInPercentage));
        if (!htmlOutput.contains("<p>"))
            htmlOutput.replace("<body>", QStringLiteral("<body style=\"line-height:%1%;\">").arg(m_textLineHeightInPercentage));
//        qDebug() << "htmlOutput 3: " << htmlOutput;
    }
    blockInfo->setTextHtml(htmlOutput);

    // Lines
    blockInfo->setLineStartPos(lineStartPos);
    blockInfo->setLineEndPos(lineEndPos);
}

void BlockModel::determineBlockIndentAndParentChildRelationship(BlockInfo* blockInfo, int positionToStartSearchFrom)
{
    if (positionToStartSearchFrom < 0 || positionToStartSearchFrom > m_blockList.length()-1)
        return;

    if (blockInfo->indentLevel() == 0 && blockInfo->parent() != nullptr) {
        blockInfo->parent()->removeChild(blockInfo);
        blockInfo->setParent(nullptr);
        return;
    }

    // We traverse the list backwards to check if this block has a parent
    for (int i = positionToStartSearchFrom; i >= 0; i--) {
        BlockInfo* previousBlock = m_blockList[i];

       // If previousBlock has a smaller total indent length than the current line/block
       // We assign it has a parent for the current block
        if (previousBlock->totalIndentLength() < blockInfo->totalIndentLength()) {

            // If parent is changing, we remove this block from its children
            if (blockInfo->parent() != nullptr && blockInfo->parent() != previousBlock) {
                blockInfo->parent()->removeChild(blockInfo);
            }

            blockInfo->setIndentLevel(previousBlock->indentLevel() + 1);
            blockInfo->setParent(previousBlock);
            previousBlock->addChild(blockInfo);
            break;
        }
    }
}

void BlockModel::updateBlockUsingPlainText(BlockInfo* blockInfo, unsigned int blockIndex, QString &plainText)
{
    unsigned int lineTotalIndentLength = calculateTotalIndentLength(plainText, blockInfo);
    blockInfo->setTotalIndentLength(lineTotalIndentLength);
    blockInfo->determineBlockType(plainText);
    if (blockInfo->blockType() == BlockInfo::BlockType::Divider)
        plainText = blockInfo->blockDelimiter();
    int lastPos = blockIndex > 0 ? m_blockList[blockIndex - 1]->lineEndPos() : -1;
    updateBlockText(blockInfo,
                    plainText,
                    lastPos + 1,
                    lastPos + 1);
    blockInfo->setIndentLevel(0);
    blockInfo->setParent(nullptr);
    if (lineTotalIndentLength > 0 && m_blockList.length() > 0) {
        determineBlockIndentAndParentChildRelationship(blockInfo, m_blockList.length() - 1);
    }
}

void BlockModel::loadText(const QString& text)
{
    QElapsedTimer timer;
    timer.start();

    emit aboutToLoadText();
    clear();
    m_sourceDocument.setPlainText(text);
    QStringList lines = text.split("\n");

    beginInsertRows(QModelIndex(), 0, lines.size() - 1);

    unsigned int blockIndex = 0;
    for (auto &line: lines) {
        BlockInfo *blockInfo = new BlockInfo(this);
        updateBlockUsingPlainText(blockInfo, blockIndex, line);
        m_blockList << blockInfo;
        blockIndex++;
    }

    endInsertRows();
    qDebug() << "Finished loading.";
    emit loadTextFinished();

    qint64 elapsed = timer.elapsed();
    qDebug() << "Time taken:" << elapsed << "ms";

}

void BlockModel::clear()
{
    beginResetModel();
    qDeleteAll(m_blockList);
    m_blockList.clear();
    endResetModel();
}

void BlockModel::updateBlocksLinePositions(unsigned int blockPosition, int delta)
{
    for (unsigned int i = blockPosition; i < m_blockList.length(); i++) {
        BlockInfo *blockInfo = m_blockList[i];
        blockInfo->setLineStartPos(blockInfo->lineStartPos() + delta);
        blockInfo->setLineEndPos(blockInfo->lineEndPos() + delta);
    }
}

QString BlockModel::QmlHtmlToMarkdown(QString &qmlHtml)
{
    // Converting QML's TextArea text (HTML) to Markdown (Plaintext)
//    qDebug() << "qmlHtml: " << qmlHtml;
    qmlHtml.replace("<br />", "\u2063"); // To preserve <br /> after toMarkdown() (otherwise it will parse these as line breaks)
    QTextDocument doc;
    doc.setHtml(qmlHtml);
    QString markdown = doc.toMarkdown();
//    qDebug() << "markdown 0: " << markdown;
    markdown.replace("\u2063", "<br />");
    markdown.replace("\n\n", ""); // due to toMarkdown() adding these
    markdown.replace("\n", " "); // due to QML's TextEdit text wrapping adding these
//    qDebug() << "markdown 1: " << markdown;

    return markdown;
}

void BlockModel::setTextAtIndex(const int blockIndex, QString qmlHtml)
{
    emit aboutToChangeText();

    BlockInfo *blockInfo = m_blockList[blockIndex];


    QString markdown = QmlHtmlToMarkdown(qmlHtml);
    // Preserve delimiter
    markdown = blockInfo->blockDelimiter() + markdown;
//    qDebug() << "markdown 2: " << markdown;
    // Preserve indentation
    if (blockInfo->totalIndentLength() > 0)
        markdown = blockInfo->indentedString() + markdown;

    if (blockInfo->textPlainText() == markdown) {
        emit textChangeFinished();
        return;
    }

//    qDebug() << "Changing!";

//    qDebug() << "markdown 3: " << markdown;

    blockInfo->determineBlockType(markdown);
    updateSourceTextBetweenLines(blockInfo->lineStartPos(), blockInfo->lineEndPos(), markdown);
    int numberOfLinesBefore = blockInfo->lineEndPos() - blockInfo->lineStartPos() + 1;
    int numberOfLinesDelta =  markdown.count('\n') + 1 - numberOfLinesBefore;
    updateBlockText(blockInfo,
                    markdown,
                    blockInfo->lineStartPos(),
                    blockInfo->lineEndPos() + numberOfLinesDelta);
    if (numberOfLinesDelta != 0) {
        updateBlocksLinePositions(blockIndex + 1, numberOfLinesDelta);
    }

    QModelIndex modelIdx = this->index(blockIndex);
    emit dataChanged(modelIdx, modelIdx, {}); // TODO: Empty vector means all roles, is that good?
    emit textChangeFinished();
}

// TODO: This function slows the app down (writing lag) when the text is very large (e.g. Moby Dick)
void BlockModel::updateSourceTextBetweenLines(unsigned int startLinePos, unsigned int endLinePos, const QString &newText)
{
//    qDebug() << "startLinePos: " << startLinePos;
//    qDebug() << "endLinePos: " << endLinePos;
//    qDebug() << "sourceDocument before: " << m_sourceDocument.toPlainText();

    QTextBlock startBlock = m_sourceDocument.findBlockByLineNumber(startLinePos);
    QTextBlock endBlock = m_sourceDocument.findBlockByLineNumber(endLinePos);
    QTextCursor cursor(startBlock);
    cursor.setPosition(endBlock.position() + endBlock.length() - 1, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(newText);

//    qDebug() << "sourceDocument after: " << m_sourceDocument.toPlainText();
}

void BlockModel::indentBlocks(QList<int> selectedBlockIndexes)
{
    std::sort(selectedBlockIndexes.begin(), selectedBlockIndexes.end());

    if (selectedBlockIndexes.length() > 0) {
        // Running on the selected blocks to check for indentation
        for (int i = selectedBlockIndexes[0]; i < selectedBlockIndexes[selectedBlockIndexes.length()-1] + 1; i++) {
            BlockInfo *blockInfo = m_blockList[i];
            if(blockInfo->isIndentable() && i > 0) {
                emit aboutToChangeText();

               // Traverse the list backwards from current block position to check for a matching parent.
               // If none found (or an empty block encountered), nothing is done.
                for (int j = i - 1; j >= 0; j--) {
                    BlockInfo *previousBlock = m_blockList[j];

                    if (previousBlock->indentLevel() < blockInfo->indentLevel()) {
                        break;
                    }

                    if (previousBlock->indentLevel() == blockInfo->indentLevel()) {

                        QString newIndentedString = previousBlock->indentedString() + '\t';
                        QString newIndentedPlainText = blockInfo->textPlainText();
                        newIndentedPlainText = newIndentedPlainText.sliced(blockInfo->indentedString().length(), newIndentedPlainText.length() - blockInfo->indentedString().length());
                        newIndentedPlainText = newIndentedString + newIndentedPlainText;

                        blockInfo->setIndentedString(newIndentedString);
                        blockInfo->setTotalIndentLength(previousBlock->totalIndentLength() + m_tabLengthInSpaces);
                        blockInfo->setIndentLevel(blockInfo->indentLevel() + 1);

                        updateSourceTextBetweenLines(blockInfo->lineStartPos(),
                                                     blockInfo->lineEndPos(),
                                                     newIndentedPlainText);

                        updateBlockText(blockInfo,
                                        newIndentedPlainText,
                                        blockInfo->lineStartPos(),
                                        blockInfo->lineEndPos());

                        // We need to run on all the children and ask them to reavaluate their parent
                        for (auto &child : blockInfo->children()) {
                            determineBlockIndentAndParentChildRelationship(child, m_blockList.indexOf(child) - 1);
                        }

                        if (blockInfo->parent() != nullptr) {
                            blockInfo->parent()->removeChild(blockInfo);
                        }
                        previousBlock->addChild(blockInfo);
                        blockInfo->setParent(previousBlock);

                        QModelIndex modelIdx = this->index(i);
                        emit dataChanged(modelIdx, modelIdx, {});

                        break;
                    }
                }

                emit textChangeFinished();
            }
        }
    }
}

void BlockModel::unindentBlock(unsigned int blockIndex, BlockInfo * blockInfo, bool isSecondRun)
{
    qDebug() << "unindentBlock!";
    emit aboutToChangeText();
    QString plainTextWithoutIndentation = blockInfo->textPlainText().mid(blockInfo->indentedString().length());
    qDebug() << "is parent null: " << (blockInfo->parent() == nullptr);
    qDebug() << "parent text: " << blockInfo->parent()->textPlainText();
    qDebug() << "parent indentedString: " << blockInfo->parent()->indentedString();
    QString newPlainText = blockInfo->parent()->indentedString() + (isSecondRun ? "\t" : "") + plainTextWithoutIndentation;

    blockInfo->setIndentedString(blockInfo->parent()->indentedString() + (isSecondRun ? "\t" : ""));
    blockInfo->setTotalIndentLength(blockInfo->parent()->totalIndentLength() + (isSecondRun ? m_tabLengthInSpaces : 0));
    blockInfo->setIndentLevel(blockInfo->parent()->indentLevel() + (isSecondRun ? 1 : 0));

    determineBlockIndentAndParentChildRelationship(blockInfo, blockIndex - 1);

    updateSourceTextBetweenLines(blockInfo->lineStartPos(),
                                 blockInfo->lineEndPos(),
                                 newPlainText);

    updateBlockText(blockInfo,
                    newPlainText,
                    blockInfo->lineStartPos(),
                    blockInfo->lineEndPos());

    QModelIndex modelIdx = this->index(blockIndex);
    emit dataChanged(modelIdx, modelIdx, {});
    emit textChangeFinished();
}

void BlockModel::unindentBlocks(QList<int> selectedBlockIndexes)
{
    qDebug() << "Unindent: " << selectedBlockIndexes;
    QList<BlockInfo*> unindentedAlready {};
    std::sort(selectedBlockIndexes.begin(), selectedBlockIndexes.end());

    if (selectedBlockIndexes.length() > 0) {
        for (int i = selectedBlockIndexes[0]; i < selectedBlockIndexes[selectedBlockIndexes.length()-1] + 1; i++) {
            BlockInfo *blockInfo = m_blockList[i];
            if(blockInfo->isIndentable() && i > 0) {
                qDebug() << "1";
                qDebug() << blockInfo->indentLevel();
                qDebug() << bool(blockInfo->parent() == nullptr);
                unsigned int originalBlockIndentLevel = blockInfo->indentLevel();
                if (blockInfo->indentLevel() > 0 && blockInfo->parent() != nullptr && !unindentedAlready.contains(blockInfo)) {
                    qDebug() << "2";
                    unindentBlock(i, blockInfo, false);
                    unindentedAlready.push_back(blockInfo);
                    int j = i + 1;
                    while (j < m_blockList.length()) {
                        qDebug() << "3";
                        BlockInfo *nextBlock = m_blockList[j];

                        if (nextBlock->indentLevel() <=  originalBlockIndentLevel){
                            break;
                        }

                        qDebug() << "4";

                        if (nextBlock->indentLevel() > 0 &&
                            nextBlock->parent() != nullptr &&
                            nextBlock->indentLevel() > originalBlockIndentLevel &&
                            !unindentedAlready.contains(nextBlock)) {
                            qDebug() << "4.5";
                            unindentBlock(j, nextBlock, true);
                            unindentedAlready.push_back(nextBlock);
                        }

                        qDebug() << "5";

                        j++;
                    }
                }
            }
        }
    }
}

void BlockModel::moveBlockTextToPreviousBlock(int blockIndex)
{
    if (blockIndex > 0) {
        BlockInfo *blockInfo = m_blockList[blockIndex];
        BlockInfo *previousBlock = m_blockList[blockIndex - 1];

        emit aboutToChangeText();
        beginRemoveRows(QModelIndex(), blockIndex, blockIndex);

        updateSourceTextBetweenLines(previousBlock->lineStartPos(),
                                     blockInfo->lineEndPos(),
                                     previousBlock->textPlainText() + blockInfo->textPlainText());

        updateBlockText(previousBlock,
                        previousBlock->textPlainText() + blockInfo->textPlainText(),
                        previousBlock->lineStartPos(),
                        previousBlock->lineEndPos());

        QModelIndex modelIdx = this->index(blockIndex - 1);
        emit dataChanged(modelIdx, modelIdx, {});

        updateBlocksLinePositions(blockIndex + 1, -1); // decrease all blocks lines data by 1

        emit textChangeFinished();
    }
}

void BlockModel::backSpaceAtStartOfBlockTextPressed(int blockIndex)
{
    BlockInfo *blockInfo = m_blockList[blockIndex];

    if (blockInfo->blockType() != BlockInfo::BlockType::RegularText && !blockInfo->blockDelimiter().isEmpty()) {
        // if this block has a delimiter -> we remove it
        emit aboutToChangeText();

        QString blockPlainText = blockInfo->textPlainText();
        QString afterIndentRemoval = blockPlainText.mid(blockInfo->indentedString().length());
        QString newBlockPlainText = afterIndentRemoval.mid(blockInfo->blockDelimiter().length());
        newBlockPlainText = blockInfo->indentedString() + newBlockPlainText;

        updateSourceTextBetweenLines(blockInfo->lineStartPos(),
                                     blockInfo->lineEndPos(),
                                     newBlockPlainText);

        blockInfo->determineBlockType(newBlockPlainText);

        updateBlockText(blockInfo,
                        newBlockPlainText,
                        blockInfo->lineStartPos(),
                        blockInfo->lineEndPos());

        QModelIndex modelIdx = this->index(blockIndex);
        emit dataChanged(modelIdx, modelIdx, {});
        emit textChangeFinished();
    } else {
        if (blockInfo->indentLevel() > 0) {
            // if this block is a regularText but indented -> we unindent it
            unindentBlocks({blockIndex});
        } else if (blockIndex > 0){
            // if this block is a regularText and not indented -> we remove it and put its text in the previous block
            beginRemoveRows(QModelIndex(), blockIndex, blockIndex);

            // We need to run on all the children and ask them to reavaluate their parent
            for (auto &child : blockInfo->children()) {
                qDebug() << "Child text: " << child->textPlainText();
                determineBlockIndentAndParentChildRelationship(child, blockIndex - 1    );
            }

            // moveBlockTextToPreviousBlock is called by the view because we want to call it before the animation

            m_blockList.removeAt(blockIndex);
            blockInfo->deleteLater();

            endRemoveRows();
        }
    }
}

void BlockModel::insertNewBlock(int blockIndex, QString qmlHtml)
{
    emit aboutToChangeText();
    beginInsertRows(QModelIndex(), blockIndex + 1, blockIndex + 1);
    BlockInfo *previousBlockInfo = m_blockList[blockIndex];
    BlockInfo *newBlockInfo = new BlockInfo(this);

    newBlockInfo->setTotalIndentLength(previousBlockInfo->totalIndentLength());
    newBlockInfo->setIndentedString(previousBlockInfo->indentedString());
    newBlockInfo->determineBlockType(previousBlockInfo->textPlainText()); // TODO: set block type and delimteter without this

    QString markdown = qmlHtml.isEmpty() ? QStringLiteral("") : QmlHtmlToMarkdown(qmlHtml);
    // If the previous block is an item in a numbered list we increase the delimiter's number by 1
    if (newBlockInfo->blockType() == BlockInfo::BlockType::NumberedListItem) {
        int indexOfDotInDelimiter = newBlockInfo->blockDelimiter().indexOf(".");
        if (indexOfDotInDelimiter != -1) {
            int previousBlockNumber = newBlockInfo->blockDelimiter().sliced(0, indexOfDotInDelimiter).toInt();
            newBlockInfo->setBlockDelimiter(QString::number(previousBlockNumber + 1) + ". ");
        }
    }

    // If the previous block is a checked task item, we need to make this an uncheked task
    if (previousBlockInfo->blockType() == BlockInfo::BlockType::Todo && previousBlockInfo->metaData()["taskChecked"].toBool()) {
        QString newTaskDelimiter = previousBlockInfo->blockDelimiter();
        newTaskDelimiter.replace("x", " ");
        newTaskDelimiter.replace("X", " ");
        newBlockInfo->setBlockDelimiter(newTaskDelimiter);
        newBlockInfo->updateMetaData("taskChecked", false);
    }

    // If the previous block is a divider or a quote and an enter is pressed, we simply create a regular empty block
    if (previousBlockInfo->blockType() == BlockInfo::BlockType::Divider ||
        previousBlockInfo->blockType() == BlockInfo::BlockType::Quote) {
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

    updateSourceTextBetweenLines(previousBlockInfo->lineStartPos(),
                                 previousBlockInfo->lineEndPos(),
                                 previousBlockInfo->textPlainText() + "\n" + markdown);

    updateBlockText(newBlockInfo,
                    markdown,
                    previousBlockInfo->lineEndPos() + 1,
                    previousBlockInfo->lineEndPos() + 1);

    newBlockInfo->setIndentLevel(previousBlockInfo->indentLevel());
    newBlockInfo->setParent(nullptr);
    if (newBlockInfo->totalIndentLength() > 0 && m_blockList.length() > 0) {
        qDebug() << "DDDDDDDDDDDDD";
        determineBlockIndentAndParentChildRelationship(newBlockInfo, blockIndex);
    }
    emit newBlockCreated(blockIndex+1);
    m_blockList.insert(blockIndex+1, newBlockInfo);
    endInsertRows();
    emit textChangeFinished();

    updateBlocksLinePositions(blockIndex + 2, 1);
}

BlockInfo::BlockType BlockModel::getBlockType(int blockIndex)
{
    if (blockIndex < 0 || blockIndex > m_blockList.length() - 1)
        return BlockInfo::BlockType::RegularText;

    BlockInfo *blockInfo = m_blockList[blockIndex];
    return blockInfo->blockType();
}

void BlockModel::toggleTaskAtIndex(int blockIndex)
{
    emit aboutToChangeText();
    BlockInfo *blockInfo = m_blockList[blockIndex];
    if (blockInfo->blockType() != BlockInfo::BlockType::Todo)
        return;

    QString newDelimiter = blockInfo->blockDelimiter();
    if (blockInfo->blockDelimiter().contains('x')) {
        newDelimiter.replace("x", " ");
        blockInfo->updateMetaData("taskChecked", false);
        qDebug() << "taskChecked: " << false;
    } else if (blockInfo->blockDelimiter().contains('X')) {
        newDelimiter.replace("X", " ");
        blockInfo->updateMetaData("taskChecked", false);
        qDebug() << "taskChecked: " << false;
    } else {
        newDelimiter.replace("[ ]", "[x]");
        blockInfo->updateMetaData("taskChecked", true);
        qDebug() << "taskChecked: " << true;
    }
    blockInfo->setBlockDelimiter(newDelimiter);

    QString plainText = blockInfo->textPlainText().mid(blockInfo->indentedString().length() + blockInfo->blockDelimiter().length());
    // New delimiter
    plainText = blockInfo->blockDelimiter() + plainText;
    // Preserve indentation
    if (blockInfo->totalIndentLength() > 0)
        plainText = blockInfo->indentedString() + plainText;

    updateBlockText(blockInfo,
                    plainText,
                    blockInfo->lineStartPos(),
                    blockInfo->lineEndPos());

    updateSourceTextBetweenLines(blockInfo->lineStartPos(), blockInfo->lineEndPos(), plainText);

    QModelIndex modelIdx = this->index(blockIndex);
    emit dataChanged(modelIdx, modelIdx, {});
    emit textChangeFinished();
}

void BlockModel::editBlocks(QList<int> selectedBlockIndexes, int firstBlockSelectionStart, int lastBlockSelectionEnd, int savedPressedChar, bool isPressedCharLower)
{
    if (selectedBlockIndexes.length() < 2)
        return;

    emit aboutToChangeText();
    std::sort(selectedBlockIndexes.begin(), selectedBlockIndexes.end());

//    BlockInfo *firstBlock = m_blockList[selectedBlockIndexes[0]];
//    qDebug() << "savedTextLastBlockQmlHtml 1: " << savedTextLastBlockQmlHtml;
//    qDebug () << "savedTextFirstBlockQmlHtml 1: " << savedTextFirstBlockQmlHtml;
//    savedTextLastBlockQmlHtml = savedTextLastBlockQmlHtml.isEmpty() ? QStringLiteral("") : QmlHtmlToMarkdown(savedTextLastBlockQmlHtml);
//    savedTextFirstBlockQmlHtml = savedTextFirstBlockQmlHtml.isEmpty() ? QStringLiteral("") : QmlHtmlToMarkdown(savedTextFirstBlockQmlHtml);
//    qDebug() << "savedTextLastBlockQmlHtml 2: " << savedTextLastBlockQmlHtml;
//    qDebug () << "savedTextFirstBlockQmlHtml 2: " << savedTextFirstBlockQmlHtml;
//    qDebug() << "savedPressedChar 1: " << savedPressedChar;
//    qDebug() << "savedPressedChar 2: " << QKeySequence(savedPressedChar).toString();
//    qDebug() << "firstBlock->textPlainText: " << firstBlock->textPlainText();

    qDebug() << "firstBlockSelectionStart: " << firstBlockSelectionStart;
    qDebug() << "lastBlockSelectionEnd: " << lastBlockSelectionEnd;

    BlockInfo *firstBlock = m_blockList[selectedBlockIndexes[0]];
    int lastBlockIndex = selectedBlockIndexes[selectedBlockIndexes.length()-1];
    BlockInfo *lastBlock = m_blockList[lastBlockIndex];
    QString savedTextFirstBlock = firstBlock->textPlainText().mid(firstBlock->indentedString().length() + firstBlock->blockDelimiter().length());
    qDebug() << "savedTextFirstBlock 1: " << savedTextFirstBlock;
    savedTextFirstBlock = savedTextFirstBlock.mid(0, firstBlockSelectionStart);
    qDebug() << "savedTextFirstBlock 2: " << savedTextFirstBlock;
    qDebug() << "lastBlock->textPlainText: " << lastBlock->textPlainText();
    int lineBreaksLength = lastBlock->textPlainText().count("<br />") * QStringLiteral("<br />").length();
    QString savedTextLastBlock = lastBlock->textPlainText().mid(lastBlock->indentedString().length() + lastBlock->blockDelimiter().length() + lastBlockSelectionEnd + lineBreaksLength);
    qDebug() << "savedTextLastBlock: " << savedTextLastBlock;

    QString savedPressedCharString = QKeySequence(savedPressedChar).toString();
    savedPressedCharString = isPressedCharLower ? savedPressedCharString.toLower() : savedPressedCharString;

    QString newPlainText = firstBlock->indentedString() + firstBlock->blockDelimiter() + savedTextFirstBlock + savedPressedCharString + savedTextLastBlock;

    qDebug() << "newPlainText: " << newPlainText;
    updateBlockText(firstBlock,
                    newPlainText,
                    firstBlock->lineStartPos(),
                    firstBlock->lineEndPos());
    QModelIndex firstBlockIndex = this->index(selectedBlockIndexes[0]);
    emit dataChanged(firstBlockIndex, firstBlockIndex, {});

    updateSourceTextBetweenLines(firstBlock->lineStartPos(), lastBlock->lineEndPos(), newPlainText);

    // Find the lowest indent level in selected blocks
    QList<int> allIndentLevels = {};
    for (int i = lastBlockIndex; i < m_blockList.length(); i++) {
        allIndentLevels.push_back(m_blockList[i]->indentLevel());
    }
    unsigned int minIndentLevel = *std::min_element(allIndentLevels.begin(), allIndentLevels.end());

    beginRemoveRows(QModelIndex(), selectedBlockIndexes[1], selectedBlockIndexes[selectedBlockIndexes.length()-1]);
    for (int i = selectedBlockIndexes[1]; i < selectedBlockIndexes[selectedBlockIndexes.length()-1]; i++) {
        BlockInfo *blockToRemove = m_blockList[i];
        if (blockToRemove->parent() != nullptr)
            blockToRemove->parent()->removeChild(blockToRemove);
        for (auto &child : blockToRemove->children()) // probably unnecessary
            child->setParent(nullptr);
        blockToRemove->deleteLater();
    }
    m_blockList.remove(selectedBlockIndexes[1], selectedBlockIndexes.length()-1);
    endRemoveRows();

    // Redetermine child parent relationship for all blocks with indent level higher then the lowest indent level found
    for (int i = selectedBlockIndexes[0] + 1; i < m_blockList.length(); i++) {
        BlockInfo *blockInfo = m_blockList[i];

        // We stop if we reach a block with an indent level equal/smaller than minIndentLevel
        if (blockInfo->indentLevel() <= minIndentLevel)
            break;

        if (blockInfo->indentLevel() > minIndentLevel) {
            determineBlockIndentAndParentChildRelationship(blockInfo, i - 1);
            QModelIndex modelIdxOfBlockIndentChanged = this->index(i);
            emit dataChanged(modelIdxOfBlockIndentChanged, modelIdxOfBlockIndentChanged, {});
        }
    }

    qDebug() << "selectedBlockIndexes: " << selectedBlockIndexes;
    qDebug() << "delta: " << -(selectedBlockIndexes.length()-1);
    updateBlocksLinePositions(selectedBlockIndexes[1], -(selectedBlockIndexes.length()-1));

    emit textChangeFinished();
}
