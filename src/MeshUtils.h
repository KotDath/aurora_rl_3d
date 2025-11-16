// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MESHUTILS_H
#define MESHUTILS_H

#include "MeshData.h"
#include <QVector3D>
#include <QVector4D>

/**
 * @brief Утилитарный класс для генерации шаблонных 3D-мешей
 *
 * MeshUtils предоставляет статические функции для создания различных
 * геометрических примитивов с настраиваемыми параметрами.
 * Все функции возвращают MeshData объекты, совместимые с существующей
 * системой рендеринга.
 */
class MeshUtils
{
public:
    /**
     * @brief Создает UV-сферу с настраиваемыми параметрами
     * @param latitudeSegments Количество сегментов по широте (минимум 3)
     * @param longitudeSegments Количество сегментов по долготе (минимум 3)
     * @param radius Радиус сферы
     * @param color Цвет вершин сферы
     * @return MeshData с треугольниками, составляющими сферу
     */
    static MeshData createUVSphere(int latitudeSegments, int longitudeSegments,
                                  float radius, const QVector4D& color);

    /**
     * @brief Создает цилиндр с настраиваемыми параметрами
     * @param radialSegments Количество радиальных сегментов (минимум 3)
     * @param radius Радиус цилиндра
     * @param height Высота цилиндра
     * @param color Цвет вершин цилиндра
     * @param includeCaps Включать ли верхнюю и нижнюю крышки
     * @return MeshData с треугольниками, составляющими цилиндр
     */
    static MeshData createCylinder(int radialSegments, float radius, float height,
                                  const QVector4D& color, bool includeCaps = true);

    /**
     * @brief Создает куб с настраиваемыми размерами
     * @param width Ширина куба (ось X)
     * @param height Высота куба (ось Y)
     * @param depth Глубина куба (ось Z)
     * @param color Цвет вершин куба
     * @return MeshData с треугольниками, составляющими куб
     */
    static MeshData createCube(float width = 1.0f, float height = 1.0f, float depth = 1.0f,
                              const QVector4D& color = QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

    /**
     * @brief Создает плоскость с настраиваемыми параметрами
     * @param width Ширина плоскости (ось X)
     * @param depth Глубина плоскости (ось Z)
     * @param widthSegments Количество сегментов по ширине
     * @param depthSegments Количество сегментов по глубине
     * @param color Цвет вершин плоскости
     * @return MeshData с треугольниками, составляющими плоскость
     */
    static MeshData createPlane(float width = 1.0f, float depth = 1.0f,
                               int widthSegments = 1, int depthSegments = 1,
                               const QVector4D& color = QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

private:
    // Вспомогательные функции для генерации геометрии
    static void addTriangle(QVector<Vertex>& vertices,
                           const QVector3D& v1, const QVector3D& v2, const QVector3D& v3,
                           const QVector4D& color, const QVector3D& normal = QVector3D(0.0f, 1.0f, 0.0f));

    static void validateSegmentCount(int segments, const QString& shapeName);
    static void validatePositiveFloat(float value, const QString& paramName);
};

#endif // MESHUTILS_H
