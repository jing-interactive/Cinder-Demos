in vec4		ciPosition;
in vec3		ciNormal;
in vec2     ciTexCoord0;

out vec3    vNormal;
out vec2    vTexCoord0;

uniform mat4 ciModelView;
uniform mat4 ciProjectionMatrix;

uniform vec2 U_CamProj;
uniform vec4 U_CylindricalProj;

vec4 cylindrical_project(vec4 modelViewPos, float fovy, float aspect, float zNear, float zFar)
{
    float halfFov = 0.5 * fovy;
    float denormY = tan(halfFov * U_CamProj.y);
    float denormX = halfFov * aspect;

    vec4 p;
    p.w = length(modelViewPos.xz);
    if (modelViewPos.z / p.w - cos(halfFov) > 0) {
        return vec4(1, 1, 1, 0);
    };

    p.xy = modelViewPos.zy / p.w;

    p.x = acos(-p.x);
    p.x = modelViewPos.x < 0.0 ? -p.x : p.x;

    p.xy *= p.w / vec2(denormX, denormY);

    p.z = (p.w - zNear) * zFar / (zFar - zNear);
    p.z = p.z * 2.0 - p.w;

    return p;
}

vec4 sphere_project(vec4 modelViewPos, float fovy, float aspect, float zNear, float zFar)
{
    float halfFov = 0.5 * fovy;
    float denormX = halfFov * aspect;
    float denormY = halfFov* U_CamProj.y;

    vec4 p;
    p.w = length(modelViewPos.xz);
    if (modelViewPos.z / p.w - cos(halfFov) > 0) {
        return vec4(1, 1, 1, 0);
    };

    p.xy = modelViewPos.zy / p.w;

    p.x = acos(-p.x);
    if (modelViewPos.x < 0.0) {
        p.x *= p.w / -denormX;
    }
    else {
        p.x *= p.w / denormX;
    }

    p.y = atan(-p.y);
    p.y *= -p.w / denormY;


    p.z = (p.w - zNear) * zFar / (zFar - zNear);
    p.z = p.z * 2.0 - p.w;

    return p;
}

vec4 project(vec4 modelViewPos)
{
    if (U_CamProj.x == 1.0) {

        return cylindrical_project(modelViewPos, U_CylindricalProj.x, U_CylindricalProj.y, U_CylindricalProj.z, U_CylindricalProj.w);
    }
    else if (U_CamProj.x == 2.0) {
        return sphere_project(modelViewPos, U_CylindricalProj.x, U_CylindricalProj.y, U_CylindricalProj.z, U_CylindricalProj.w);
    }
    else {
        return ciProjectionMatrix * modelViewPos;
    }
}

void main()
{
	gl_Position = project(ciModelView * ciPosition);

    vNormal = ciNormal;
    vTexCoord0.x = ciTexCoord0.x;
    vTexCoord0.y = 1 - ciTexCoord0.y;
}
