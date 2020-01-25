#include "gcpocketraster.h"

#include "gcfile.h"

#include <QElapsedTimer>
#include <point.h>

namespace GCode {
RasterCreator::RasterCreator()
{
}

void RasterCreator::create()
{
    if (m_gcp.dParam[AccDistance].toDouble() != 0.0)
        createRaster2(m_gcp.tool.first(), m_gcp.dParam[Depth].toDouble(), m_gcp.dParam[UseAngle].toDouble(), m_gcp.dParam[Pass].toInt());
    else
        createRaster(m_gcp.tool.first(), m_gcp.dParam[Depth].toDouble(), m_gcp.dParam[UseAngle].toDouble(), m_gcp.dParam[Pass].toInt());
}

void RasterCreator::createRaster(const Tool& tool, const double depth, const double angle, const int prPass)
{
    enum {
        NoProfilePass,
        First,
        Last,
    };
    QElapsedTimer t;
    t.start();
    if (m_gcp.side == On) {
        emit fileReady(nullptr);
        return;
    }

    m_toolDiameter = tool.getDiameter(depth) * uScale;
    m_dOffset = m_toolDiameter / 2;
    m_stepOver = tool.stepover() * uScale;

    Paths profilePaths;
    Paths fillPaths;

    switch (m_gcp.side) {
    case Outer:
        groupedPaths(CutoffPaths, static_cast<cInt>(m_toolDiameter + 5));
        if (m_groupedPss.size() > 1 && m_groupedPss.first().size() == 2)
            m_groupedPss.removeFirst();
        break;
    case Inner:
        groupedPaths(CopperPaths);
        break;
    default:;
    }
    for (Paths src : m_groupedPss) {
        if (prPass) {
            ClipperOffset offset(uScale);
            offset.AddPaths(src, jtRound, etClosedPolygon);
            offset.Execute(src, -m_dOffset);
            profilePaths.append(src);
            fillPaths.append(src);
        }

        if (src.size()) {
            for (Path& path : src)
                path.append(path.first());
            ClipperOffset offset(uScale);
            offset.AddPaths(src, jtRound, etClosedPolygon);
            offset.Execute(src, prPass ? -m_dOffset * 1.05 : -m_dOffset);
        } else
            continue;

        if (src.size()) {
            for (Path& path : src) {
                path.append(path.first());
            }

            Clipper clipper;
            clipper.AddPaths(src, ptClip, true);
            const IntRect r(clipper.GetBounds());
            clipper.Clear();
            const cInt size = static_cast<cInt>(Length({ r.left, r.top }, { r.right, r.bottom }));
            const cInt end = static_cast<cInt>(r.bottom + (size - (r.bottom - r.top)) * 0.5);
            const cInt start = static_cast<cInt>(r.top - (size - (r.bottom - r.top)) * 0.5);
            const cInt left = static_cast<cInt>(r.left - (size - (r.right - r.left)) * 0.5);
            const cInt right = static_cast<cInt>(r.right + (size - (r.right - r.left)) * 0.5);
            const IntPoint center(static_cast<cInt>(0.5 * (r.left + r.right)), static_cast<cInt>(0.5 * (r.top + r.bottom)));

            Paths acc;
            using Worck = QVector<std::tuple<cInt, cInt, cInt, cInt>>;
            /////////////////////////////////////////////////////////
            std::function<void(Worck)> scan = [this, &angle, &center, &src, &acc](Worck w) {
                static QMutex m;
                auto [_1, _2, _3, flag] = w.first();
                Q_UNUSED(_1)
                Q_UNUSED(_2)
                Q_UNUSED(_3)
                Paths scanLine;
                {
                    Clipper clipper;
                    clipper.AddPaths(src, ptClip, true);
                    for (auto& [left_, right_, var, flag] : w) {
                        Q_UNUSED(flag)
                        Path frame { { left_, var }, { right_, var } };
                        RotatePath(frame, angle, center);
                        clipper.AddPath(frame, ptSubject, false);
                    }
                    clipper.Execute(ctIntersection, scanLine, pftPositive);
                    if (qFuzzyCompare(angle, 90)) {
                        if (!flag) {
                            for (Path& path : scanLine)
                                if (path.first().Y > path.last().Y)
                                    ReversePath(path);
                        } else {
                            for (Path& path : scanLine)
                                if (path.first().Y < path.last().Y)
                                    ReversePath(path);
                        }
                    } else {
                        if (!flag) {
                            for (Path& path : scanLine)
                                if (path.first().X > path.last().X)
                                    ReversePath(path);
                        } else {
                            for (Path& path : scanLine)
                                if (path.first().X < path.last().X)
                                    ReversePath(path);
                        }
                    }
                }
                {

                    Paths toNext;
                    Clipper clipper;
                    clipper.AddPaths(src, ptSubject, false);
                    for (auto [left_, right_, var, flag] : w) {
                        Q_UNUSED(flag)
                        Path frame {
                            { left_, var },
                            { right_, var },
                            { right_, var += m_stepOver },
                            { left_, var }
                        };
                        RotatePath(frame, angle, center);
                        clipper.AddPath(frame, ptClip, true);
                    }
                    clipper.Execute(ctIntersection, toNext, pftPositive);
                    mergeSegments(toNext);
                    if (scanLine.isEmpty()) {
                        QMutexLocker l(&m);
                        acc.append(toNext);
                    } else {
                        for (Path& dst : scanLine) {
                            for (int i = 0; i < toNext.size(); ++i) {
                                Path& src = toNext[i];
                                if (dst.last() == src.first()) {
                                    dst.append(src.mid(1));
                                    toNext.remove(i--);
                                } else if (dst.last() == src.last()) {
                                    ReversePath(src);
                                    dst.append(src.mid(1));
                                    toNext.remove(i--);
                                } else if (dst.first() == src.first()) {
                                    toNext.remove(i--);
                                } else if (dst.first() == src.last()) {
                                    toNext.remove(i--);
                                }
                            }
                        }
                        QMutexLocker l(&m);
                        acc.append(scanLine);
                        acc.append(toNext);
                    }
                }
            };
            /////////////////////////////////////////////////////////

            QVector<Worck> map;

            if (0) {
                for (cInt var = start, flag = 0; var < end + m_stepOver * 5; flag = (flag ? 0 : 1), var += flag ? m_stepOver : m_stepOver * 5) {
                    map.append({ { left, right, var, flag },
                        { left, right, static_cast<cInt>(var + m_stepOver * 2), flag },
                        { left, right, static_cast<cInt>(var + m_stepOver * 4), flag } });
                }
            } else {
                for (cInt var = start, flag = 0; var < end; flag = (flag ? 0 : 1), var += m_stepOver) {
                    map.append({ { left, right, var, flag } });
                }
            }
            if (0) {
                m_progressMax += m_groupedPss.size() + map.size();
                for (int i = 0; i < map.size(); ++i) {
                    scan(map[i]);
                    ++m_progressVal;
                }
            } else {
                m_progressMax += m_groupedPss.size() + map.size();
                for (int i = 0, c = QThread::idealThreadCount(); i < map.size(); i += c) {
                    auto m(map.mid(i, c));
                    QFuture<void> future = QtConcurrent::map(m, scan);
                    future.waitForFinished();
                    m_progressVal += m.size();
                }
            }

            if (!acc.isEmpty()) {
                mergeSegments(acc);
                m_returnPs.append(acc);
                //sortB(m_returnPss.last());
            }
        }
    }

    sortB(m_returnPs);
    if (!profilePaths.isEmpty() && prPass) {
        sortB(profilePaths);
        if (m_gcp.convent)
            ReversePaths(profilePaths);
        for (Path& path : profilePaths)
            path.append(path.first());
    }

    switch (prPass) {
    case NoProfilePass:
        m_returnPss.prepend(m_returnPs);
        break;
    case First:
        if (!profilePaths.isEmpty())
            m_returnPss.prepend(profilePaths);
        m_returnPss.prepend(m_returnPs);
        break;
    case Last:
        m_returnPss.prepend(m_returnPs);
        if (!profilePaths.isEmpty())
            m_returnPss.append(profilePaths);
        break;
    default:
        break;
    }
    qDebug() << "createRaster" << (t.elapsed() / 1000);
    if (m_returnPss.isEmpty()) {
        emit fileReady(nullptr);
    } else {
        m_file = new File(m_returnPss, tool, depth, Raster, fillPaths);
        m_file->setFileName(tool.name());
        emit fileReady(m_file);
    }
}

void RasterCreator::createRaster2(const Tool& tool, const double depth, const double angle, const int prPass)
{
    enum {
        NoProfilePass,
        First,
        Last,
    };
    QElapsedTimer t;
    t.start();
    if (m_gcp.side == On) {
        emit fileReady(nullptr);
        return;
    }

    m_toolDiameter = tool.getDiameter(depth) * uScale;
    m_dOffset = m_toolDiameter / 2;
    m_stepOver = tool.stepover() * uScale;

    Paths profilePaths;
    Paths fillPaths;

    switch (m_gcp.side) {
    case Outer:
        groupedPaths(CutoffPaths, uScale);
        if (m_groupedPss.size() > 1 && m_groupedPss.first().size() == 2)
            m_groupedPss.removeFirst();
        break;
    case Inner:
        groupedPaths(CopperPaths);
        break;
    default:;
    }
    IntRect r;

    {
        Clipper c;
        for (auto& p : m_groupedPss)
            c.AddPaths(p, ptClip, true);
        r = c.GetBounds();
    }
    int c {};
    Path rrr;
    progress(10, ++c);
    qDebug() << m_gcp.dParam[AccDistance].toDouble() /** uScale*/ * 2;
    r.left -= 5 * uScale; // m_gcp.dParam[AccDistance].toDouble() * uScale * 2;
    r.right += 5 * uScale; // m_gcp.dParam[AccDistance].toDouble() * uScale * 2;
    for (cInt i = r.top; i <= r.bottom; /*++i*/) {
        rrr.append({ { r.left, i }, { r.right, i } });
        i += tool.stepover() * uScale;
        rrr.append({ { r.right, i }, { r.left, i } });
        i += tool.stepover() * uScale;
    }
    progress(10, ++c);
    Paths laserPath;

    {
        Paths tempPath;
        ClipperOffset o;
        for (auto& p : m_groupedPss)
            o.AddPaths(p, jtRound, etClosedPolygon);
        o.Execute(profilePaths, -tool.diameter() * uScale * 0.5);

        progress(10, ++c);
        {
            Clipper c;
            c.AddPath(rrr, ptSubject, false);
            c.AddPaths(profilePaths, ptClip, true);
            c.Execute(ctDifference, tempPath, pftNonZero);
            for (auto& p : tempPath) {
                if (auto s = p.size(); s > 4)
                    p.remove(2, s - 4);
            }
            laserPath.append(tempPath);
        }
        progress(10, ++c);
        {
            Clipper c;
            c.AddPath(rrr, ptSubject, false);
            c.AddPaths(profilePaths, ptClip, true);
            c.Execute(ctIntersection, tempPath, pftNonZero);
            laserPath.append(tempPath);
        }

        progress(10, ++c);

        auto sortBE2 = [r](Paths& src) {
            IntPoint startPt { r.left, r.top };
            for (int idx = 0; idx < src.size(); ++idx) {
                if (startPt == src[idx].first()) {
                    std::swap(src[0], src[idx]);
                    startPt = src[0].last();
                    break;
                }
            }
            for (int firstIdx = 1; firstIdx < src.size(); ++firstIdx) {
                progress(src.size(), firstIdx);
                int swapIdx = firstIdx;
                double destLen = std::numeric_limits<double>::max();
                bool reverse = false;
                for (int secondIdx = firstIdx; secondIdx < src.size(); ++secondIdx) {
                    const double lenFirst = Length(startPt, src[secondIdx].first());
                    const double lenLast = Length(startPt, src[secondIdx].last());
                    if (lenFirst < lenLast) {
                        if (destLen > lenFirst) {
                            destLen = lenFirst;
                            swapIdx = secondIdx;
                            reverse = false;
                        }
                    } else {
                        if (destLen > lenLast) {
                            destLen = lenLast;
                            swapIdx = secondIdx;
                            reverse = true;
                        }
                    }
                    if (qFuzzyIsNull(destLen))
                        break;
                }
                if (reverse)
                    ReversePath(src[swapIdx]);
                startPt = src[swapIdx].last();
                if (swapIdx != firstIdx)
                    std::swap(src[firstIdx], src[swapIdx]);
            }
        };

        sortBE2(laserPath /*, psss.first().last()*/ /*{ r.left, r.top }*/);

        for (int i = 0; i < laserPath.size() - 1; ++i) {
            if (double l = Length(laserPath[i].last(), laserPath[i + 1].first()); l > 1)
                qDebug() << "sortBE2 err" << i << l;
        }
    }
    progress(10, ++c);
    m_returnPss.append(laserPath);
    if (!profilePaths.isEmpty() && prPass != NoProfilePass) {
        for (auto& p : profilePaths) {
            p.append(p.first());
        }
        m_returnPss.append(sortB(profilePaths));
    }

    qDebug() << "createRaster" << (t.elapsed() / 1000);
    if (m_returnPss.isEmpty()) {
        emit fileReady(nullptr);
    } else {
        m_file = new File(m_returnPss, tool, depth, Laser, fillPaths);
        m_file->setFileName(tool.name());
        emit fileReady(m_file);
    }
}

//Paths& RasterCreator::sortBE(Paths& src)
//{
//    IntPoint startPt(toIntPoint(Marker::get(Marker::Home)->pos() + Marker::get(Marker::Zero)->pos()));
//    for (int firstIdx = 0; firstIdx < src.size(); ++firstIdx) {
//        progress(src.size(), firstIdx);
//        int swapIdx = firstIdx;
//        double destLen = std::numeric_limits<double>::max();
//        bool reverse = false;
//        for (int secondIdx = firstIdx; secondIdx < src.size(); ++secondIdx) {
//            const double lenFirst = Length(startPt, src[secondIdx].first());
//            const double lenLast = Length(startPt, src[secondIdx].last());
//            if (lenFirst < lenLast) {
//                if (destLen > lenFirst) {
//                    destLen = lenFirst;
//                    swapIdx = secondIdx;
//                    reverse = false;
//                }
//            } else {
//                if (destLen > lenLast) {
//                    destLen = lenLast;
//                    swapIdx = secondIdx;
//                    reverse = true;
//                }
//            }
//        }
//        if (reverse)
//            ReversePath(src[swapIdx]);
//        startPt = src[swapIdx].last();
//        if (swapIdx != firstIdx)
//            std::swap(src[firstIdx], src[swapIdx]);
//    }
//    return src;
//}
}
