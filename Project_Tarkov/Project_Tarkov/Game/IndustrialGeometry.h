#pragma once
#include "IndustrialZone.h"
#include "../Graphics/Mesh.h"
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <tuple>
#include <limits>

namespace IndustrialZone
{
    struct Batch {
        Mesh mesh;
        glm::vec3 color{1}, low{0}, high{0};
    };

    // Low-poly primitives have real surface normals, including faceted rocks.
    inline std::vector<Vertex> Primitive(Shape shape)
    {
        std::vector<Vertex> vertices;
        auto tri = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
            const auto cross=glm::cross(b-a,c-a);
            if (glm::length(cross)<.000001f) return;
            const auto normal=glm::normalize(cross);
            for (auto p : {a,b,c}) vertices.push_back({p,normal,{0,0}});
        };
        if (shape==Shape::Box) {
            const glm::vec3 v[]={{-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{-.5f,.5f,-.5f},
                {-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f}};
            const int faces[][4]={{0,3,2,1},{4,5,6,7},{0,4,7,3},{1,2,6,5},{3,7,6,2},{0,1,5,4}};
            for (auto& f : faces) { tri(v[f[0]],v[f[1]],v[f[2]]); tri(v[f[0]],v[f[2]],v[f[3]]); }
        } else if (shape==Shape::Rock) {
            constexpr int sides=9;
            glm::vec3 rings[3][sides];
            for (int ring=0;ring<3;++ring) for (int j=0;j<sides;++j) {
                const float angle=6.2831853f*j/sides+ring*.19f;
                const float radius=(ring==1?.5f:.34f)*( .85f+.15f*std::sin(float(j*7+ring*3)) );
                rings[ring][j]={radius*std::cos(angle),-.35f+ring*.35f,radius*std::sin(angle)};
            }
            for (int j=0;j<sides;++j) {
                int next=(j+1)%sides;
                tri({0,-.5f,0},rings[0][j],rings[0][next]);
                tri({.07f,.5f,0},rings[2][next],rings[2][j]);
                for (int r=0;r<2;++r) {
                    tri(rings[r][j],rings[r+1][j],rings[r+1][next]);
                    tri(rings[r][j],rings[r+1][next],rings[r][next]);
                }
            }
        } else {
            constexpr int sides=12;
            for (int j=0;j<sides;++j) {
                float a=6.2831853f*j/sides, b=6.2831853f*(j+1)/sides;
                glm::vec3 lowA(.5f*std::cos(a),-.5f,.5f*std::sin(a));
                glm::vec3 lowB(.5f*std::cos(b),-.5f,.5f*std::sin(b));
                tri({0,-.5f,0},lowA,lowB);
                if (shape==Shape::Cone) tri(lowA,{0,.5f,0},lowB);
                else {
                    auto highA=lowA+glm::vec3(0,1,0), highB=lowB+glm::vec3(0,1,0);
                    tri(lowA,highA,highB); tri(lowA,highB,lowB);
                    tri({0,.5f,0},highB,highA);
                }
            }
        }
        return vertices;
    }

    inline void Bake(const std::vector<Block>& blocks, std::vector<Batch>& batches)
    {
        batches.clear();
        struct Pending {
            std::vector<Vertex> vertices;
            glm::vec3 color{1};
            glm::vec3 low{(std::numeric_limits<float>::max)()}, high{-(std::numeric_limits<float>::max)()};
        };
        using Key=std::tuple<int,int,int,int,int>;
        std::map<Key,Pending> groups;
        const std::array<std::vector<Vertex>,4> primitives{{Primitive(Shape::Box),Primitive(Shape::Cylinder),Primitive(Shape::Cone),Primitive(Shape::Rock)}};
        for (const auto& block : blocks) {
            const Key key{static_cast<int>(std::floor(block.center.x/50)),static_cast<int>(std::floor(block.center.z/50)),
                static_cast<int>(block.color.r*255),static_cast<int>(block.color.g*255),static_cast<int>(block.color.b*255)};
            auto& group=groups[key]; group.color=block.color;
            glm::mat4 transform=glm::translate(glm::mat4(1),block.center);
            transform=glm::rotate(transform,glm::radians(block.rotation.x),{1,0,0});
            transform=glm::rotate(transform,glm::radians(block.rotation.y),{0,1,0});
            transform=glm::rotate(transform,glm::radians(block.rotation.z),{0,0,1});
            transform=glm::scale(transform,block.size);
            const auto normals=glm::transpose(glm::inverse(glm::mat3(transform)));
            for (auto vertex : primitives[static_cast<std::size_t>(block.shape)]) {
                vertex.pos=glm::vec3(transform*glm::vec4(vertex.pos,1));
                vertex.normal=glm::normalize(normals*vertex.normal);
                group.low=(glm::min)(group.low,vertex.pos); group.high=(glm::max)(group.high,vertex.pos);
                group.vertices.push_back(vertex);
            }
        }
        batches.reserve(groups.size());
        for (auto& entry : groups) {
            auto& group=entry.second;
            batches.emplace_back(); auto& batch=batches.back();
            batch.color=group.color; batch.low=group.low; batch.high=group.high;
            batch.mesh.Create(group.vertices);
        }
    }
}
