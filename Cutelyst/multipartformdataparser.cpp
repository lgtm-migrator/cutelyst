/*
 * Copyright (C) 2014-2016 Daniel Nicoletti <dantti12@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "multipartformdataparser_p.h"
#include "upload_p.h"
#include "common.h"

using namespace Cutelyst;

Uploads MultiPartFormDataParser::parse(QIODevice *body, const QString &contentType, int bufferSize)
{
    Uploads ret;
    int start = contentType.indexOf(QLatin1String("boundary="));
    if (start == -1) {
        qCWarning(CUTELYST_MULTIPART) << "No boudary match" << contentType;
        return ret;
    }

    start += 9;
    QByteArray boundary;
    const int len = contentType.length();
    boundary.reserve(contentType.length() - start + 2);

    for (int i = start, quotes = 0; i < len; ++i) {
        const QChar ch = contentType.at(i);
        if (ch == QLatin1Char('\"')) {
            if ((quotes == 0 && i > start) || ++quotes == 2) {
                break;
            }
        } else if (ch == QLatin1Char(';')) {
            break;
        } else {
            boundary.append(ch.toLatin1());
        }
    }

    if (boundary.isEmpty()) {
        qCWarning(CUTELYST_MULTIPART) << "Boudary match was empty" << contentType;
        return ret;
    }
    boundary.prepend("--");

    if (bufferSize < 1024) {
        bufferSize = 1024;
    }
    char *buffer = new char[bufferSize];

    ret = MultiPartFormDataParserPrivate::execute(buffer, bufferSize, body, boundary);

    delete [] buffer;

    return ret;
}

Uploads MultiPartFormDataParserPrivate::execute(char *buffer, int bufferSize, QIODevice *body, const QByteArray &boundary)
{
    Uploads ret;
    QByteArray headerLine;
    Headers headers;
    qint64 startOffset;
    int bufferSkip = 0;
    int boundaryPos = 0;
    int boundarySize = boundary.size();
    ParserState state = FindBoundary;
    QByteArrayMatcher matcher(boundary);

    while (!body->atEnd()) {
        qint64 len = body->read(buffer + bufferSkip, bufferSize - bufferSkip) + bufferSkip;
        bufferSkip = 0;
        int i = 0;
        while (i < len) {
            switch (state) {
            case FindBoundary:
                i += findBoundary(buffer + i, len - i, matcher, boundarySize, state, boundaryPos);
                break;
            case EndBoundaryCR:
                // TODO the "--" case
                if (buffer[i] != '\r') {
//                    qCDebug(CUTELYST_MULTIPART) << "EndBoundaryCR return!";
                    return ret;
                }
                state = EndBoundaryLF;
                break;
            case EndBoundaryLF:
                if (buffer[i] != '\n') {
//                    qCDebug(CUTELYST_MULTIPART) << "EndBoundaryLF return!";
                    return ret;
                }
                state = StartHeaders;
                break;
            case StartHeaders:
                if (headerLine.isEmpty() && buffer[i] == '\r') {
                    // nothing was read
                    state = EndHeaders;
                } else {
                    char *pch = strchr(buffer + i, '\r');
                    if (pch == NULL) {
                        headerLine.append(buffer + i, len - i);
                        i = len;
                    } else {
                        headerLine.append(buffer + i, pch - buffer - i);
                        i = pch - buffer;
                        state = FinishHeader;
                    }
                }
                break;
            case FinishHeader:
                if (buffer[i] == '\n') {
                    int dotdot = headerLine.indexOf(':');
                    headers.setHeader(QString::fromLatin1(headerLine.left(dotdot)),
                                      QString::fromLatin1(headerLine.mid(dotdot + 1).trimmed()));
                    headerLine = QByteArray();
                    state = StartHeaders;
                } else {
//                    qCDebug(CUTELYST_MULTIPART) << "FinishHeader return!";
                    return ret;
                }
                break;
            case EndHeaders:
                if (buffer[i] == '\n') {
                    state = StartData;
                } else {
//                    qCDebug(CUTELYST_MULTIPART) << "EndHeaders return!";
                    return ret;
                }
                break;
            case StartData:
//                qCDebug(CUTELYST_MULTIPART) << "StartData" << body->pos() - len + i;
                startOffset = body->pos() - len + i;
                state = EndData;
            case EndData:
                i += findBoundary(buffer + i, len - i, matcher, boundarySize, state, boundaryPos);
                if (state == EndBoundaryCR) {
//                    qCDebug(CUTELYST_MULTIPART) << "EndData" << body->pos() - len + i - boundaryLength - 1;
                    ret.append(new Upload(new UploadPrivate(body, headers, startOffset, body->pos() - len + i - boundarySize - 1)));
                    headers = Headers();
                } else if (body->atEnd()) {
                    // Boundary was not found and we are at the end
                    return ret;
                } else {
                    // Boundary was not found so move the boundary size at end of the buffer
                    // to be sure we don't have the boundary in the middle of two chunks
                    bufferSkip = boundarySize - 1;
                    memmove(buffer, buffer + len - bufferSkip, bufferSkip);
                    break;
                }
            }
            ++i;
        }
    }

    return ret;
}

int MultiPartFormDataParserPrivate::findBoundary(char *buffer, int len, const QByteArrayMatcher &matcher, int boundarySize, MultiPartFormDataParserPrivate::ParserState &state, int &boundaryPos)
{
    int i = matcher.indexIn(buffer, len);
    //    qCDebug(CUTELYST_MULTIPART) << "findBoundary" << QByteArray(buffer, len);
    if (i != -1) {
        //        qCDebug(CUTELYST_MULTIPART) << "FindBoundary: found at" << i << body->pos() << len << body->pos() - len + i << i + boundaryLength;
        boundaryPos = 0;
        state = EndBoundaryCR;
        return i + boundarySize - 1;
    }
    return len;
}
