/*
 *  This file is part of Permafrost Engine. 
 *  Copyright (C) 2017 Eduard Permyakov 
 *
 *  Permafrost Engine is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Permafrost Engine is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "render_gl.h"
#include "render_private.h"
#include "mesh.h"
#include "vertex.h"
#include "shader.h"
#include "material.h"
#include "../entity.h"
#include "../gl_uniforms.h"
#include "../anim/public/skeleton.h"
#include "../anim/public/anim.h"
#include "../map/public/tile.h"

#include <GL/glew.h>

#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>


#define ARR_SIZE(a) (sizeof(a)/sizeof(a[0]))

/*****************************************************************************/
/* STATIC FUNCTIONS                                                          */
/*****************************************************************************/

static void r_gl_set_materials(GLuint shader_prog, size_t num_mats, const struct material *mats)
{
    for(size_t i = 0; i < num_mats; i++) {
    
        const struct material *mat = &mats[i];
        const size_t nmembers = 3; 

        const struct member_desc{
            const GLchar *name; 
            size_t        size;
            ptrdiff_t     offset;
        }descs[] = {
            {"ambient_intensity", 1, offsetof(struct material, ambient_intensity) },
            {"diffuse_clr",       3, offsetof(struct material, diffuse_clr)       },
            {"specular_clr",      3, offsetof(struct material, specular_clr)      }
        };

        for(size_t j = 0; j < nmembers; j++) {
        
            char locbuff[64];
            GLuint loc;

            snprintf(locbuff, sizeof(locbuff), "%s[%zu].%s", GL_U_MATERIALS, i, descs[j].name);
            locbuff[sizeof(locbuff)-1] = '\0';

            loc = glGetUniformLocation(shader_prog, locbuff);
            switch(descs[j].size) {
            case 1: glUniform1fv(loc, 1, (void*) ((char*)mat + descs[j].offset) ); break;
            case 3: glUniform3fv(loc, 1, (void*) ((char*)mat + descs[j].offset) ); break;
            default: assert(0);
            }

        }
    }
}

static void r_gl_set_uniform_mat4x4_array(mat4x4_t *data, size_t count, 
                                          const char *uname, const char *shader_name)
{
    GLuint loc, shader_prog;

    shader_prog = R_Shader_GetProgForName(shader_name);
    glUseProgram(shader_prog);

    loc = glGetUniformLocation(shader_prog, uname);
    glUniformMatrix4fv(loc, count, GL_FALSE, (void*)data);
}

static void r_gl_set_uniform_vec4_array(vec4_t *data, size_t count, 
                                        const char *uname, const char *shader_name)
{
    GLuint loc, shader_prog;

    shader_prog = R_Shader_GetProgForName(shader_name);
    glUseProgram(shader_prog);

    loc = glGetUniformLocation(shader_prog, uname);
    glUniform4fv(loc, count, (void*)data);
}

static void r_gl_set_view(const mat4x4_t *view, const char *shader_name)
{
    GLuint loc, shader_prog;

    shader_prog = R_Shader_GetProgForName(shader_name);
    glUseProgram(shader_prog);

    loc = glGetUniformLocation(shader_prog, GL_U_VIEW);
    glUniformMatrix4fv(loc, 1, GL_FALSE, view->raw);
}

static void r_gl_set_proj(const mat4x4_t *proj, const char *shader_name)
{
    GLuint loc, shader_prog;

    shader_prog = R_Shader_GetProgForName(shader_name);
    glUseProgram(shader_prog);

    loc = glGetUniformLocation(shader_prog, GL_U_PROJECTION);
    glUniformMatrix4fv(loc, 1, GL_FALSE, proj->raw);
}

static void r_gl_set_view_pos(const vec3_t *pos, const char *shader_name)
{
    GLuint loc, shader_prog;

    shader_prog = R_Shader_GetProgForName(shader_name);
    glUseProgram(shader_prog);

    loc = glGetUniformLocation(shader_prog, GL_U_VIEW_POS);
    glUniform3fv(loc, 1, pos->raw);
}


/*****************************************************************************/
/* EXTERN FUNCTIONS                                                          */
/*****************************************************************************/

void R_GL_Init(struct render_private *priv)
{
    struct mesh *mesh = &priv->mesh;

    glGenVertexArrays(1, &mesh->VAO);
    glBindVertexArray(mesh->VAO);

    glGenBuffers(1, &mesh->VBO);
    glBindBuffer(GL_ARRAY_BUFFER, mesh->VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh->num_verts * sizeof(struct vertex), mesh->vbuff, GL_STATIC_DRAW);

    /* Attribute 0 - position */
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)0);
    glEnableVertexAttribArray(0);

    /* Attribute 1 - texture coordinates */
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), 
        (void*)offsetof(struct vertex, uv));
    glEnableVertexAttribArray(1);

    /* Attribute 2 - normal */
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct vertex), 
        (void*)offsetof(struct vertex, normal));
    glEnableVertexAttribArray(2);

    /* Attribute 3 - material index */
    glVertexAttribIPointer(3, 1, GL_INT, sizeof(struct vertex), 
        (void*)offsetof(struct vertex, material_idx));
    glEnableVertexAttribArray(3);

    /* Attribute 4 - joint indices */
    glVertexAttribPointer(4, 4, GL_INT, GL_FALSE, sizeof(struct vertex),
        (void*)offsetof(struct vertex, joint_indices));
    glEnableVertexAttribArray(4);  

    /* Attribute 5 - joint weights */
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(struct vertex),
        (void*)offsetof(struct vertex, weights));
    glEnableVertexAttribArray(5);  

    priv->shader_prog = R_Shader_GetProgForName("mesh.animated.textured");
}

void R_GL_Draw(const void *render_private, mat4x4_t *model)
{
    const struct render_private *priv = render_private;
    GLuint loc;

    glUseProgram(priv->shader_prog);

    loc = glGetUniformLocation(priv->shader_prog, GL_U_MODEL);
    glUniformMatrix4fv(loc, 1, GL_FALSE, model->raw);

    r_gl_set_materials(priv->shader_prog, priv->num_materials, priv->materials);

    for(int i = 0; i < priv->num_materials; i++) {
        R_Texture_GL_Activate(&priv->materials[i].texture, priv->shader_prog);
    }

    glBindVertexArray(priv->mesh.VAO);
    glDrawArrays(GL_TRIANGLES, 0, priv->mesh.num_verts);
}

void R_GL_SetViewMatAndPos(const mat4x4_t *view, const vec3_t *pos)
{
    const char *shaders[] = {
        "mesh.static.colored",
        "mesh.animated.textured",
        "mesh.animated.normals.colored",
    };

    for(int i = 0; i < ARR_SIZE(shaders); i++) {

        r_gl_set_view(view, shaders[i]);
        r_gl_set_view_pos(pos, shaders[i]);
    }
}

void R_GL_SetProj(const mat4x4_t *proj, const char *shader_name)
{
    const char *shaders[] = {
        "mesh.static.colored",
        "mesh.animated.textured",
        "mesh.animated.normals.colored",
    };

    for(int i = 0; i < ARR_SIZE(shaders); i++)
        r_gl_set_proj(proj, shaders[i]);
}

void R_GL_SetAnimUniformMat4x4Array(mat4x4_t *data, size_t count, const char *uname)
{
    const char *shaders[] = {
        "mesh.animated.textured",
        "mesh.animated.normals.colored",
    };

    for(int i = 0; i < ARR_SIZE(shaders); i++)
        r_gl_set_uniform_mat4x4_array(data, count, uname, shaders[i]);
}

void R_GL_SetAnimUniformVec4Array(vec4_t *data, size_t count, const char *uname)
{
    const char *shaders[] = {
        "mesh.animated.textured",
        "mesh.animated.normals.colored",
    };

    for(int i = 0; i < ARR_SIZE(shaders); i++)
        r_gl_set_uniform_vec4_array(data, count, uname, shaders[i]);
}

void R_GL_SetAmbientLightColor(vec3_t color)
{
    const char *shaders[] = {
        "mesh.animated.textured",
    };

    for(int i = 0; i < ARR_SIZE(shaders); i++) {
    
        GLuint loc, shader_prog;

        shader_prog = R_Shader_GetProgForName(shaders[i]);
        glUseProgram(shader_prog);

        loc = glGetUniformLocation(shader_prog, GL_U_AMBIENT_COLOR);
        glUniform3fv(loc, 1, color.raw);
    }
}

void R_GL_SetLightEmitColor(vec3_t color)
{
    const char *shaders[] = {
        "mesh.animated.textured",
    };

    for(int i = 0; i < ARR_SIZE(shaders); i++) {
    
        GLuint loc, shader_prog;

        shader_prog = R_Shader_GetProgForName(shaders[i]);
        glUseProgram(shader_prog);

        loc = glGetUniformLocation(shader_prog, GL_U_LIGHT_COLOR);
        glUniform3fv(loc, 1, color.raw);
    }
}

void R_GL_SetLightPos(vec3_t pos)
{
    const char *shaders[] = {
        "mesh.animated.textured",
    };

    for(int i = 0; i < ARR_SIZE(shaders); i++) {

        GLuint loc, shader_prog;
    
        shader_prog = R_Shader_GetProgForName(shaders[i]);
        glUseProgram(shader_prog);

        loc = glGetUniformLocation(shader_prog, GL_U_LIGHT_POS);
        glUniform3fv(loc, 1, pos.raw);
    }
}

void R_GL_DrawSkeleton(const struct entity *ent, const struct skeleton *skel)
{
    vec3_t *vbuff;
    GLint VAO, VBO;
    GLint shader_prog;
    GLuint loc;
    vec3_t green = (vec3_t){0.0f, 1.0f, 0.0f};

    /* Our vbuff looks like this:
     * +----------------+-------------+--------------+-----
     * | joint root 0   | joint tip 0 | joint root 1 | ...
     * +----------------+-------------+--------------+-----
     */
    vbuff = calloc(skel->num_joints * 2, sizeof(vec3_t));

    for(int i = 0, vbuff_idx = 0; i < skel->num_joints; i++, vbuff_idx +=2) {

        struct joint *curr = &skel->joints[i];
        struct SQT *sqt = &skel->bind_sqts[i];

        vec4_t homo = (vec4_t){0.0f, 0.0f, 0.0f, 1.0f}; 
        vec4_t result;

        mat4x4_t bind_pose;
        PFM_Mat4x4_Inverse(&skel->inv_bind_poses[i], &bind_pose);

        /* The root of the bone in object space */
        PFM_Mat4x4_Mult4x1(&bind_pose, &homo, &result);
        vbuff[vbuff_idx] = (vec3_t){result.x, result.y ,result.z};
    
        /* The tip of the bone in object space */
        homo = (vec4_t){curr->tip.x, curr->tip.y, curr->tip.z, 1.0f}; 
        PFM_Mat4x4_Mult4x1(&bind_pose, &homo, &result);
        vbuff[vbuff_idx + 1] = (vec3_t){result.x, result.y ,result.z};
    }
 
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, skel->num_joints * sizeof(vec3_t) * 2, vbuff, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3_t), (void*)0);
    glEnableVertexAttribArray(0);  

    shader_prog = R_Shader_GetProgForName("mesh.static.colored");
    glUseProgram(shader_prog);

    /* Set uniforms */
    loc = glGetUniformLocation(shader_prog, GL_U_COLOR);
    glUniform3fv(loc, 1, green.raw);

    loc = glGetUniformLocation(shader_prog, GL_U_MODEL);

    mat4x4_t model;
    Entity_ModelMatrix(ent, &model);
    glUniformMatrix4fv(loc, 1, GL_FALSE, model.raw);

    glPointSize(5.0f);

    glBindVertexArray(VAO);
    glDrawArrays(GL_POINTS, 0, skel->num_joints * 2);
    glDrawArrays(GL_LINES, 0, skel->num_joints * 2);

    free(vbuff);
}

void R_GL_DrawOrigin(const void *render_private, mat4x4_t *model)
{
    vec3_t vbuff[2];
    GLint VAO, VBO;
    GLint shader_prog;
    GLuint loc;

    vec3_t red   = (vec3_t){1.0f, 0.0f, 0.0f};
    vec3_t green = (vec3_t){0.0f, 1.0f, 0.0f};
    vec3_t blue  = (vec3_t){0.0f, 0.0f, 1.0f};

    /* OpenGL setup */
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3_t), (void*)0);
    glEnableVertexAttribArray(0);  

    shader_prog = R_Shader_GetProgForName("mesh.static.colored");
    glUseProgram(shader_prog);

    /* Set uniforms */
    loc = glGetUniformLocation(shader_prog, GL_U_MODEL);
    glUniformMatrix4fv(loc, 1, GL_FALSE, model->raw);

    /* Set line width */
    GLfloat old_width;
    glGetFloatv(GL_LINE_WIDTH, &old_width);
    glLineWidth(3.0f);

    /* Render the 3 axis lines at the origin */
    vbuff[0] = (vec3_t){0.0f, 0.0f, 0.0f};
    loc = glGetUniformLocation(shader_prog, GL_U_COLOR);

    for(int i = 0; i < 3; i++) {

        switch(i) {
        case 0:
            vbuff[1] = (vec3_t){1.0f, 0.0f, 0.0f}; 
            glUniform3fv(loc, 1, red.raw);
            break;
        case 1:
            vbuff[1] = (vec3_t){0.0f, 1.0f, 0.0f}; 
            glUniform3fv(loc, 1, green.raw);
            break;
        case 2:
            vbuff[1] = (vec3_t){0.0f, 0.0f, 1.0f}; 
            glUniform3fv(loc, 1, blue.raw);
            break;
        }
    
        glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(vec3_t), vbuff, GL_STATIC_DRAW);

        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES, 0, 2);
    }
    glLineWidth(old_width);
}

void R_GL_DrawNormals(const void *render_private, mat4x4_t *model)
{
    const struct render_private *priv = render_private;

    GLuint normals_shader = R_Shader_GetProgForName("mesh.animated.normals.colored");
    assert(normals_shader);
    glUseProgram(normals_shader);

    GLuint loc;
    vec3_t yellow = (vec3_t){1.0f, 1.0f, 0.0f};

    loc = glGetUniformLocation(normals_shader, GL_U_COLOR);
    glUniform3fv(loc, 1, yellow.raw);

    loc = glGetUniformLocation(normals_shader, GL_U_MODEL);
    glUniformMatrix4fv(loc, 1, GL_FALSE, model->raw);

    glBindVertexArray(priv->mesh.VAO);
    glDrawArrays(GL_TRIANGLES, 0, priv->mesh.num_verts);
}

void R_GL_VerticesFromTile(const struct tile *tile, struct vertex *out, size_t r, size_t c)
{
    /* We take the directions to be relative to a normal vector facing outwards
     * from the plane of the face. West is to the right, east is to the left,
     * north is top, south is bottom. */
    struct face{
        struct vertex nw, ne, se, sw; 
    };

    /* Bottom face is always the same (just shifted over based on row and column), and the 
     * front, back, left, right faces just connect the top and bottom faces. The only 
     * variations are in the top face, which has some corners raised based on tile type. */

    struct face bot = {
        .nw = (struct vertex) {
            .pos    = (vec3_t) { 0.0f - ((c+1) * X_COORDS_PER_TILE), (-1.0f * Y_COORDS_PER_TILE), 
                                 0.0f + (r * Z_COORDS_PER_TILE) },
            .uv     = (vec2_t) { 0.0f, 1.0f },
            .normal = (vec3_t) { 0.0f, -1.0f, 0.0f },
            .material_idx  = tile->top_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .ne = (struct vertex) {
            .pos    = (vec3_t) { 0.0f - (c * X_COORDS_PER_TILE), (-1.0f * Y_COORDS_PER_TILE), 
                                 0.0f + (r * Z_COORDS_PER_TILE) }, 
            .uv     = (vec2_t) { 1.0f, 1.0f },
            .normal = (vec3_t) { 0.0f, -1.0f, 0.0f },
            .material_idx  = tile->top_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .se = (struct vertex) {
            .pos    = (vec3_t) { 0.0f - (c * X_COORDS_PER_TILE), (-1.0f * Y_COORDS_PER_TILE), 
                                 0.0f + ((r+1) * Z_COORDS_PER_TILE) }, 
            .uv     = (vec2_t) { 1.0f, 0.0f },
            .normal = (vec3_t) { 0.0f, -1.0f, 0.0f },
            .material_idx  = tile->top_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .sw = (struct vertex) {
            .pos    = (vec3_t) { 0.0f - ((c+1) * X_COORDS_PER_TILE), (-1.0f * Y_COORDS_PER_TILE), 
                                 0.0f + ((r+1) * Z_COORDS_PER_TILE) }, 
            .uv     = (vec2_t) { 0.0f, 0.0f },
            .normal = (vec3_t) { 0.0f, -1.0f, 0.0f },
            .material_idx  = tile->top_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
    };

    bool top_nw_raised =  (tile->type == TILETYPE_RAMP_SN)
                       || (tile->type == TILETYPE_RAMP_EW)
                       || (tile->type == TILETYPE_CORNER_CONVEX_SW)
                       || (tile->type == TILETYPE_CORNER_CONVEX_SE)
                       || (tile->type == TILETYPE_CORNER_CONCAVE_SE);

    bool top_ne_raised =  (tile->type == TILETYPE_RAMP_SN)
                       || (tile->type == TILETYPE_RAMP_WE)
                       || (tile->type == TILETYPE_CORNER_CONVEX_SW)
                       || (tile->type == TILETYPE_CORNER_CONCAVE_SW)
                       || (tile->type == TILETYPE_CORNER_CONVEX_SE);

    bool top_sw_raised =  (tile->type == TILETYPE_RAMP_NS)
                       || (tile->type == TILETYPE_RAMP_EW)
                       || (tile->type == TILETYPE_CORNER_CONVEX_SE);

    bool top_se_raised =  (tile->type == TILETYPE_RAMP_NS)
                       || (tile->type == TILETYPE_RAMP_WE)
                       || (tile->type == TILETYPE_CORNER_CONVEX_SW);

    struct face top = {
        .nw = (struct vertex) {
            .pos    = (vec3_t) { 0.0f - (c * X_COORDS_PER_TILE),
                                 (tile->base_height * Y_COORDS_PER_TILE)
                                 + (Y_COORDS_PER_TILE * (top_nw_raised ? tile->ramp_height : 0)),
                                 0.0f + (r * Z_COORDS_PER_TILE) },
            .uv     = (vec2_t) { 0.0f, 1.0f },
            .normal = (vec3_t) { 0.0f, 1.0f, 0.0f },
            .material_idx  = tile->top_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .ne = (struct vertex) {
            .pos    = (vec3_t) { 0.0f - ((c+1) * X_COORDS_PER_TILE), 
                                 (tile->base_height * Y_COORDS_PER_TILE)
                                 + (Y_COORDS_PER_TILE * (top_ne_raised ? tile->ramp_height : 0)), 
                                 0.0f + (r * Z_COORDS_PER_TILE) }, 
            .uv     = (vec2_t) { 1.0f, 1.0f },
            .normal = (vec3_t) { 0.0f, 1.0f, 0.0f },
            .material_idx  = tile->top_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .se = (struct vertex) {
            .pos    = (vec3_t) { 0.0f - ((c+1) * X_COORDS_PER_TILE), 
                                 (tile->base_height * Y_COORDS_PER_TILE)
                                 + (Y_COORDS_PER_TILE * (top_se_raised ? tile->ramp_height : 0)), 
                                 0.0f + ((r+1) * Z_COORDS_PER_TILE) }, 
            .uv     = (vec2_t) { 1.0f, 0.0f },
            .normal = (vec3_t) { 0.0f, 1.0f, 0.0f },
            .material_idx  = tile->top_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .sw = (struct vertex) {
            .pos    = (vec3_t) { 0.0f - (c * X_COORDS_PER_TILE), 
                                 (tile->base_height * Y_COORDS_PER_TILE)
                                 + (Y_COORDS_PER_TILE * (top_sw_raised ? tile->ramp_height : 0)), 
                                 0.0f + ((r+1) * Z_COORDS_PER_TILE) }, 
            .uv     = (vec2_t) { 0.0f, 0.0f },
            .normal = (vec3_t) { 0.0f, 1.0f, 0.0f },
            .material_idx  = tile->top_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
    };

#define V_COORD(width, height) (((float)height)/width)

    struct face back = {
        .nw = (struct vertex) {
            .pos    = top.nw.pos,
            .uv     = (vec2_t) { 0.0f, V_COORD(X_COORDS_PER_TILE, back.nw.pos.y) },
            .normal = (vec3_t) { 0.0f, 0.0f, -1.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .ne = (struct vertex) {
            .pos    = top.ne.pos,
            .uv     = (vec2_t) { 1.0f, V_COORD(X_COORDS_PER_TILE, back.ne.pos.y) },
            .normal = (vec3_t) { 0.0f, 0.0f, -1.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .se = (struct vertex) {
            .pos    = bot.nw.pos,
            .uv     = (vec2_t) { 1.0f, 0.0f },
            .normal = (vec3_t) { 0.0f, 0.0f, -1.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .sw = (struct vertex) {
            .pos    = bot.ne.pos,
            .uv     = (vec2_t) { 0.0f, 0.0f },
            .normal = (vec3_t) { 0.0f, 0.0f, -1.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
    };

    struct face front = {
        .nw = (struct vertex) {
            .pos    = top.sw.pos,
            .uv     = (vec2_t) { 0.0f, V_COORD(X_COORDS_PER_TILE, front.nw.pos.y) },
            .normal = (vec3_t) { 0.0f, 0.0f, 1.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .ne = (struct vertex) {
            .pos    = top.se.pos,
            .uv     = (vec2_t) { 1.0f, V_COORD(X_COORDS_PER_TILE, front.ne.pos.y) },
            .normal = (vec3_t) { 0.0f, 0.0f, 1.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .se = (struct vertex) {
            .pos    = bot.sw.pos,
            .uv     = (vec2_t) { 1.0f, 0.0f },
            .normal = (vec3_t) { 0.0f, 0.0f, 1.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .sw = (struct vertex) {
            .pos    = bot.se.pos,
            .uv     = (vec2_t) { 0.0f, 0.0f },
            .normal = (vec3_t) { 0.0f, 0.0f, 1.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
    };

    struct face left = {
        .nw = (struct vertex) {
            .pos    = top.sw.pos,
            .uv     = (vec2_t) { 0.0f, V_COORD(X_COORDS_PER_TILE, left.nw.pos.y) },
            .normal = (vec3_t) { 1.0f, 0.0f, 0.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .ne = (struct vertex) {
            .pos    = top.nw.pos,
            .uv     = (vec2_t) { 1.0f, V_COORD(X_COORDS_PER_TILE, left.ne.pos.y) },
            .normal = (vec3_t) { 1.0f, 0.0f, 0.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .se = (struct vertex) {
            .pos    = bot.ne.pos,
            .uv     = (vec2_t) { 1.0f, 0.0f },
            .normal = (vec3_t) { 1.0f, 0.0f, 0.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .sw = (struct vertex) {
            .pos    = bot.se.pos,
            .uv     = (vec2_t) { 0.0f, 0.0f },
            .normal = (vec3_t) { 1.0f, 0.0f, 0.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
    };

    struct face right = {
        .nw = (struct vertex) {
            .pos    = top.ne.pos,
            .uv     = (vec2_t) { 0.0f, V_COORD(X_COORDS_PER_TILE, right.nw.pos.y) },
            .normal = (vec3_t) { -1.0f, 0.0f, 0.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .ne = (struct vertex) {
            .pos    = top.se.pos,
            .uv     = (vec2_t) { 1.0f, V_COORD(X_COORDS_PER_TILE, right.ne.pos.y) },
            .normal = (vec3_t) { -1.0f, 0.0f, 0.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .se = (struct vertex) {
            .pos    = bot.sw.pos,
            .uv     = (vec2_t) { 1.0f, 0.0f },
            .normal = (vec3_t) { -1.0f, 0.0f, 0.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
        .sw = (struct vertex) {
            .pos    = bot.nw.pos,
            .uv     = (vec2_t) { 0.0f, 0.0f },
            .normal = (vec3_t) { -1.0f, 0.0f, 0.0f },
            .material_idx  = tile->sides_mat_idx,
            .joint_indices = {0},
            .weights       = {0}
        },
    };

#undef V_COORD

    struct face *faces[] = {
        &top, &bot, &front, &back, &left, &right 
    };

    for(int i = 0; i < ARR_SIZE(faces); i++) {
    
        struct face *curr = faces[i];

        /* First triangle */
        memcpy(out + (i * VERTS_PER_FACE) + 0, &curr->nw, sizeof(struct vertex));
        memcpy(out + (i * VERTS_PER_FACE) + 1, &curr->ne, sizeof(struct vertex));
        memcpy(out + (i * VERTS_PER_FACE) + 2, &curr->sw, sizeof(struct vertex));

        /* Second triangle */
        memcpy(out + (i * VERTS_PER_FACE) + 3, &curr->se, sizeof(struct vertex));
        memcpy(out + (i * VERTS_PER_FACE) + 4, &curr->sw, sizeof(struct vertex));
        memcpy(out + (i * VERTS_PER_FACE) + 5, &curr->ne, sizeof(struct vertex));
    }
}

