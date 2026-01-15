// SPDX-FileCopyrightText: 2025 Open Mobile Platform LLC <community@omp.ru>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef MESHDATA_H
#define MESHDATA_H

#include <QVector3D>
#include <QVector4D>
#include <QVector2D>
#include <QVector>
#include <QtOpenGL/QGLFunctions>

struct Vertex
{
    QVector3D position;
    QVector4D color;
    QVector3D normal;

    Vertex() {}
    Vertex(const QVector3D& pos, const QVector4D& col, const QVector3D& norm = QVector3D(0.0f, 1.0f, 0.0f))
        : position(pos), color(col), normal(norm) {}
};

enum class MaterialType
{
    VertexColorPhong = 0,
    Checkerboard
};

struct MaterialSettings
{
    MaterialType type = MaterialType::VertexColorPhong;
    QVector4D colorLight = QVector4D(1.0f, 1.0f, 1.0f, 1.0f);
    QVector4D colorDark = QVector4D(0.0f, 0.0f, 0.0f, 1.0f);
    QVector2D tiling = QVector2D(1.0f, 1.0f);
};

class MeshData
{
public:
    MeshData();
    MeshData(const QVector<Vertex>& vertices, GLenum primitiveType = GL_TRIANGLES,
             const MaterialSettings& material = MaterialSettings());

    const QVector<Vertex>& vertices() const { return m_vertices; }
    void setVertices(const QVector<Vertex>& vertices) { m_vertices = vertices; }

    GLenum primitiveType() const { return m_primitiveType; }
    void setPrimitiveType(GLenum type) { m_primitiveType = type; }

    int vertexCount() const { return m_vertices.size(); }

    const MaterialSettings& materialSettings() const { return m_material; }
    void setMaterialSettings(const MaterialSettings& settings) { m_material = settings; }
    MaterialType materialType() const { return m_material.type; }
    void setMaterialType(MaterialType type) { m_material.type = type; }

    static MeshData createTriangle(const QVector3D& p1, const QVector3D& p2, const QVector3D& p3,
                                  const QVector4D& color = QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
    static MeshData createColoredTriangle(const QVector3D& p1, const QVector3D& p2, const QVector3D& p3,
                                          const QVector4D& color1, const QVector4D& color2, const QVector4D& color3);

    static MeshData createColoredCube(float size = 1.0f);
    static MeshData createColoredCube(float size, const QVector4D& baseColor);
    static MeshData createPlane(float width, float depth, const QVector4D& color);

private:
    QVector<Vertex> m_vertices;
    GLenum m_primitiveType;
    MaterialSettings m_material;
};

#endif // MESHDATA_H
