/*
    SPDX-FileCopyrightText: 2015 Meltytech LLC
    SPDX-FileCopyrightText: 2015 Brian Matherly <code@brianmatherly.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef DATAQUEUE_H
#define DATAQUEUE_H

#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>

/*!
  \class DataQueue
  \brief The DataQueue provides a thread safe container for passing data between
  objects.

  threadsafe

  DataQueue provides a limited size container for passing data between objects.
  One object can add data to the queue by calling push() while another object
  can remove items from the queue by calling pop().

  DataQueue provides configurable behavior for handling overflows. It can
  discard the oldest, discard the newest or block the object calling push()
  until room has been freed in the queue by another object calling pop().

  DataQueue is threadsafe and is therefore most appropriate when passing data
  between objects operating in different thread contexts.
*/

template <class T> class DataQueue
{
public:
    //! Overflow behavior modes.
    typedef enum {
        OverflowModeDiscardOldest = 0, //!< Discard oldest items
        OverflowModeDiscardNewest,     //!< Discard newest items
        OverflowModeWait               //!< Wait for space to be free
    } OverflowMode;

    /*!
      Constructs a DataQueue.

      The \a size will be the maximum queue size and the \a mode will dictate
      overflow behavior.
    */
    explicit DataQueue(int maxSize, OverflowMode mode);

    //! Destructs a DataQueue.
    virtual ~DataQueue();

    /*!
      Pushes an item into the queue.

      If the queue is full and overflow mode is OverflowModeWait then this
      function will block until pop() is called.
    */
    void push(const T &item);

    /*!
      Pops an item from the queue.

      If the queue is empty then this  function will block. If blocking is
      undesired, then check the return of count() before calling pop().
    */
    T pop();

    //! Returns the number of items in the queue.
    int count() const;

private:
    QList<T> m_queue;
    int m_maxSize;
    OverflowMode m_mode;
    mutable QMutex m_mutex;
    QWaitCondition m_notEmptyCondition;
    QWaitCondition m_notFullCondition;
};

template <class T>
DataQueue<T>::DataQueue(int maxSize, OverflowMode mode)
    : m_queue()
    , m_maxSize(maxSize)
    , m_mode(mode)
    , m_mutex()
    , m_notEmptyCondition()
    , m_notFullCondition()
{
}

template <class T> DataQueue<T>::~DataQueue() = default;

template <class T> void DataQueue<T>::push(const T &item)
{
    m_mutex.lock();
    if (m_queue.size() == m_maxSize) {
        switch (m_mode) {
        case OverflowModeDiscardOldest:
            m_queue.removeFirst();
            m_queue.append(item);
            break;
        case OverflowModeDiscardNewest:
            // This item is the newest so discard it and exit
            break;
        case OverflowModeWait:
            m_notFullCondition.wait(&m_mutex);
            m_queue.append(item);
            break;
        }
    } else {
        m_queue.append(item);
        if (m_queue.size() == 1) {
            m_notEmptyCondition.wakeOne();
        }
    }
    m_mutex.unlock();
}

template <class T> T DataQueue<T>::pop()
{
    T retVal;
    m_mutex.lock();
    if (m_queue.size() == 0) {
        m_notEmptyCondition.wait(&m_mutex);
    }
    retVal = m_queue.takeFirst();
    if (m_mode == OverflowModeWait && m_queue.size() == m_maxSize - 1) {
        m_notFullCondition.wakeOne();
    }
    m_mutex.unlock();
    return retVal;
}

template <class T> int DataQueue<T>::count() const
{
    QMutexLocker locker(&m_mutex);
    return m_queue.size();
}

#endif // DATAQUEUE_H
