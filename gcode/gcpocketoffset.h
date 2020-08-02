#pragma once
//#ifndef POCKET_H
//#define POCKET_H

#include "gccreator.h"

namespace GCode {
class PocketCreator : public Creator {
public:
    PocketCreator();

private:
    void createPocket(const Tool& tool, const double depth, const int steps);
    void createPocket2(QVector<Tool>& tools, double depth);

protected:
    void create() override; // Creator interface
};
}

//#endif // POCKET_H
