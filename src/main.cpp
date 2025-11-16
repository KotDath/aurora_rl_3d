// SPDX-FileCopyrightText: 2024 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#include <auroraapp.h>
#include <QtQuick>

#include "RenderWindow.h"
#include "SceneObject.h"
#include "TrainingTypes.h"

#include <rl_tools/operations/cpu.h>
#include <rl_tools/containers/tensor/tensor.h>
#include <rl_tools/containers/tensor/operations_generic.h>
#include <rl_tools/containers/tensor/operations_cpu.h>
#include <iostream>

namespace rlt = rl_tools;

int main(int argc, char *argv[])
{
    qRegisterMetaType<SceneProfile>("SceneProfile");
    qRegisterMetaType<AntTrainingMetrics>("AntTrainingMetrics");

    using DEVICE = rlt::devices::DefaultCPU;
    using Scalar = float;
    using Index = typename DEVICE::index_t;
    DEVICE device;

    using Shape = rlt::tensor::Shape<Index, 4, 4, 4>;
    using Stride = rlt::tensor::RowMajorStride<Shape>;
    rlt::Tensor<rlt::tensor::Specification<Scalar, Index, Shape, true, Stride>> tensor;

    rlt::malloc(device, tensor);
    rlt::set_all(device, tensor, Scalar{2});
    const Scalar sum = rlt::sum(device, tensor);
    std::cout << "tensor sum: " << sum << std::endl;
    rlt::free(device, tensor);


    QScopedPointer<QGuiApplication> application(Aurora::Application::application(argc, argv));
    application->setOrganizationName(QStringLiteral("ru.kotdath"));
    application->setApplicationName(QStringLiteral("AuroraRL3D"));

    // Register QML types
    qmlRegisterType<RenderWindow>("AuroraRL3D", 1, 0, "RenderWindow");
    qmlRegisterType<SceneObject>("AuroraRL3D", 1, 0, "SceneObject");

    QScopedPointer<QQuickView> view(Aurora::Application::createView());
    view->setSource(Aurora::Application::pathTo(QStringLiteral("qml/AuroraRL3D.qml")));
    view->show();

    return application->exec();
}
