#include "core/booklistmodel.h"

static const int BOOK_ROLE = Qt::UserRole + 1;

// Placeholder row: empty, but with every field BookRow reads
static const QVariantMap &placeholder()
{
    static const QVariantMap book{
        {"loaded", false},
        {"id", -1},
        {"title", QString()},
        {"authors", QStringList()},
        {"statusText", QString()},
        {"seriesText", QString()},
        {"hasCover", false},
        {"coverVersion", QString()},
        {"sectionStart", false},
        {"sectionLabel", QString()},
        {"variants", QVariantList()},
        {"formats", QStringList()},
        {"hidden", false},
    };
    return book;
}

int BookListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_books.size();
}

QVariant BookListModel::data(const QModelIndex &index, int role) const
{
    if (role != BOOK_ROLE || !index.isValid() || index.row() >= m_books.size())
        return QVariant();
    const QVariant &book = m_books.at(index.row());
    return book.isValid() ? book : QVariant(placeholder());
}

QHash<int, QByteArray> BookListModel::roleNames() const
{
    return {{BOOK_ROLE, "book"}};
}

bool BookListModel::isLoaded(int row) const
{
    return row >= 0 && row < m_books.size() && m_books.at(row).isValid();
}

QVariantMap BookListModel::book(int row) const
{
    return isLoaded(row) ? m_books.at(row).toMap() : QVariantMap();
}

void BookListModel::reset(int count)
{
    beginResetModel();
    m_books = QVariantList(count);
    endResetModel();
}

void BookListModel::setBooks(int first, const QVariantList &books)
{
    const int last = qMin(first + int(books.size()), int(m_books.size())) - 1;
    if (first < 0 || last < first)
        return;
    for (int row = first; row <= last; row++) {
        QVariantMap book = books.at(row - first).toMap();
        book.insert("loaded", true);
        m_books[row] = book;
    }
    emit dataChanged(index(first), index(last));
}

void BookListModel::setField(int row, const QString &name, const QVariant &value)
{
    if (!isLoaded(row))
        return;
    QVariantMap book = m_books.at(row).toMap();
    if (book.value(name) == value)
        return;
    book.insert(name, value);
    m_books[row] = book;
    emit dataChanged(index(row), index(row));
}
