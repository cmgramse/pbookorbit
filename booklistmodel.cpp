#include "booklistmodel.h"

static const int BOOK_ROLE = Qt::UserRole + 1;

int BookListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_books.size();
}

QVariant BookListModel::data(const QModelIndex &index, int role) const
{
    if (role != BOOK_ROLE || !index.isValid() || index.row() >= m_books.size())
        return QVariant();
    return m_books.at(index.row());
}

QHash<int, QByteArray> BookListModel::roleNames() const
{
    return {{BOOK_ROLE, "book"}};
}

QVariantMap BookListModel::last() const
{
    return m_books.isEmpty() ? QVariantMap() : m_books.last().toMap();
}

void BookListModel::append(const QVariantList &books)
{
    if (books.isEmpty())
        return;
    beginInsertRows(QModelIndex(), m_books.size(), m_books.size() + books.size() - 1);
    m_books += books;
    endInsertRows();
}

void BookListModel::clear()
{
    if (m_books.isEmpty())
        return;
    beginResetModel();
    m_books.clear();
    endResetModel();
}
