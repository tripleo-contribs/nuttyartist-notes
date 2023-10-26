#include "blockinfo.h"

BlockInfo::BlockInfo(QObject *parent)
    : QObject{parent},
      m_textHtml(""),
      m_textPlainText(""),
      m_blockType(BlockType::RegularText),
      m_lineStartPos(0),
      m_lineEndPos(0),
      m_totalIndentLength(0),
      m_indentLevel(0),
      m_children({}),
      m_parent(nullptr),
      m_blockDelimiter(""),
      m_indentedString(""),
      m_metaData({})
{
}

QString BlockInfo::textHtml() const
{
    return m_textHtml;
}

void BlockInfo::setTextHtml(const QString &newTextHtml)
{
    if (m_textHtml == newTextHtml)
        return;
    m_textHtml = newTextHtml;
    emit textHtmlChanged();
}

QString BlockInfo::textPlainText() const
{
    return m_textPlainText;
}

void BlockInfo::setTextPlainText(const QString &newTextPlainText)
{
    if (m_textPlainText == newTextPlainText)
        return;
    m_textPlainText = newTextPlainText;
    emit textPlainTextChanged();
}

BlockInfo::BlockType BlockInfo::blockType() const
{
    return m_blockType;
}

void BlockInfo::setBlockType(const BlockType &newType)
{
    if (m_blockType == newType)
        return;

    m_blockType = newType;
    emit blockTypeChanged();
}

bool BlockInfo::isNumberedList(QString str) {
    // Check if the string starts with a number
    int index = 0;
    while (index < str.length() && str[index].isDigit()) {
        index++;
    }

    // Check if the next characters are ". "
    if (index > 0 && index + 1 < str.length() && str.sliced(index, 2) == ". ") {
        setBlockDelimiter(str.sliced(0, index + 2));
        return true;
    }

    return false;
}

QString BlockInfo::indentedString() const
{
    return m_indentedString;
}

void BlockInfo::setIndentedString(const QString &newIndentedString)
{
    if (m_indentedString == newIndentedString)
        return;
    m_indentedString = newIndentedString;
    emit indentedStringChanged();
}

QString BlockInfo::trimLeadingWhitespaces(const QString &str) {
    int i;
    for (i = 0; i < str.length(); i++) {
        if (!str[i].isSpace()) {
            break;
        }
    }
    return str.mid(i);
}

BlockInfo::BlockType BlockInfo::determineBlockType(QString text)
{
    text = trimLeadingWhitespaces(text);

    static const QStringList headingPrefixes = {"# ", "## ", "### ", "#### ", "##### ", "###### "};
    for (const QString &prefix : headingPrefixes) {
        if (text.startsWith(prefix)) {
            setBlockDelimiter(prefix);
            setBlockType(BlockType::Heading);
            return BlockType::Heading;
        }
    }

    static const QStringList quotePrefixes = {"| ", "> ", ">> ", ">>> "};
    for (const QString &prefix : quotePrefixes) {
        if (text.startsWith(prefix)) {
            setBlockDelimiter(prefix);
            setBlockType(BlockType::Quote);
            return BlockType::Quote;
        }
    }

    static const QStringList todoItemPrefixes = {"[ ] ", "[x] ", "- [ ] ", "* [ ] ", "+ [ ] ", "- [x] ", "* [x] ", "+ [x] "};
    for (const QString &prefix : todoItemPrefixes) {
        if (text.startsWith(prefix)) {
            if (text[1] == 'x' || text[1] == 'X' || text[3] == 'x' || text[3] == 'X') {
                m_metaData["taskChecked"] = true;
            } else {
                m_metaData["taskChecked"] = false;
            }
            if (m_metaData.isEmpty()) {
                qDebug() << "m_metaData IS EMPTY!";
            }
            emit metaDataChanged();
            setBlockDelimiter(prefix);
            setBlockType(BlockType::Todo);
            return BlockType::Todo;
        }
    }

    static const QStringList bulletItemPrefixes = {"- ", "* ", "+ "};
    for (const QString &prefix : bulletItemPrefixes) {
        if (text.startsWith(prefix)) {
            setBlockDelimiter(prefix);
            setBlockType(BlockType::BulletListItem);
            return BlockType::BulletListItem;
        }
    }

    if (isNumberedList(text)) {
        setBlockType(BlockType::NumberedListItem);
        return BlockType::NumberedListItem;
    }

    if (text.startsWith("---")) {
        setBlockDelimiter("---");
        setBlockType(BlockType::Divider);
        return BlockType::Divider;
    }

    if (text.startsWith("^") && text.length() >= 2) {
        setBlockDelimiter("^" + text.sliced(1, 1).toUpper());
        setBlockType(BlockType::DropCap);
        return BlockType::DropCap;
    }

    // TODO: Kanban, Column, Images, Code blocks

    setBlockDelimiter("");
    setBlockType(BlockType::RegularText);
    return BlockType::RegularText;
}

unsigned int BlockInfo::lineStartPos() const
{
    return m_lineStartPos;
}

void BlockInfo::setLineStartPos(unsigned int newLineStartPos)
{
    if (m_lineStartPos == newLineStartPos)
        return;
    if (newLineStartPos > 100000) {
        qDebug() << "TOO BIG 14";
    }
    m_lineStartPos = newLineStartPos;
    emit lineStartPosChanged();
}

unsigned int BlockInfo::lineEndPos() const
{
    return m_lineEndPos;
}

void BlockInfo::setLineEndPos(unsigned int newLineEndPos)
{
    if (m_lineEndPos == newLineEndPos)
        return;
    m_lineEndPos = newLineEndPos;
    emit lineEndPosChanged();
}

unsigned int BlockInfo::totalIndentLength() const
{
    return m_totalIndentLength;
}

void BlockInfo::setTotalIndentLength(unsigned int newTotalIndentLength)
{
    if (m_totalIndentLength == newTotalIndentLength)
        return;
    m_totalIndentLength = newTotalIndentLength;
    emit totalIndentLengthChanged();
}

QList<BlockInfo *> BlockInfo::children() const
{
    return m_children;
}

void BlockInfo::setChildren(const QList<BlockInfo *> &newChildren)
{
    if (m_children == newChildren)
        return;
    m_children = newChildren;
    emit childrenChanged();
}

void BlockInfo::addChild(BlockInfo *newChild)
{
    if (!m_children.contains(newChild)) {
        m_children.append(newChild);
        emit childrenChanged();
    }
}

void BlockInfo::removeChild(BlockInfo *child)
{
    if (m_children.contains(child)) {
        m_children.removeOne(child);
        emit childrenChanged();
    }
}

BlockInfo *BlockInfo::parent() const
{
    return m_parent;
}

void BlockInfo::setParent(BlockInfo *newParent)
{
    if (m_parent == newParent)
        return;
    m_parent = newParent;
    emit parentChanged();
}

unsigned int BlockInfo::indentLevel() const
{
    return m_indentLevel;
}

void BlockInfo::setIndentLevel(unsigned int newIndentLevel)
{
    if (m_indentLevel == newIndentLevel)
        return;
    m_indentLevel = newIndentLevel;
    emit indentLevelChanged();
}

bool BlockInfo::isBlockListItem()
{
    return m_blockType == BlockType::BulletListItem ||
            m_blockType == BlockType::NumberedListItem ||
            m_blockType == BlockType::Todo;
}

QString BlockInfo::blockDelimiter() const
{
    return m_blockDelimiter;
}

void BlockInfo::setBlockDelimiter(const QString &newBlockDelimiter)
{
    if (m_blockDelimiter == newBlockDelimiter)
        return;
    m_blockDelimiter = newBlockDelimiter;
    emit blockDelimiterChanged();
}

bool BlockInfo::isIndentable()
{
    if (m_blockType == BlockType::RegularText ||
        m_blockType == BlockType::BulletListItem ||
        m_blockType == BlockType::NumberedListItem ||
        m_blockType == BlockType::Todo ||
        m_blockType == BlockType::Heading ||
        m_blockType == BlockType::Quote ||
        m_blockType == BlockType::Divider) {
        return true;
    }

    return false;
}

QJsonObject BlockInfo::metaData() const
{
    return m_metaData;
}

void BlockInfo::setMetaData(const QJsonObject &newMetaData)
{
    if (m_metaData == newMetaData)
        return;
    m_metaData = newMetaData;
    emit metaDataChanged();
}

void BlockInfo::updateMetaData(const QString key, const QVariant value)
{
    m_metaData[key] = QJsonValue::fromVariant(value);
    emit metaDataChanged();
}
