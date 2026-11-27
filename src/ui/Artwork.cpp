#include "ui/Artwork.h"
#include "render/GLHeaders.h"
#include <cstdint>
#include <fstream>
#include <vector>

namespace uiart {
namespace {
GLuint githubTexture=0,secretTexture=0;float githubAspect=1;
std::uint32_t read32(std::ifstream& stream) {
    unsigned char bytes[4] = {};
    stream.read(reinterpret_cast<char*>(bytes), 4);
    return bytes[0] | (std::uint32_t(bytes[1]) << 8) | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}
}

void init(const std::string& directory) {
    std::ifstream secret(directory+"/secret-message.rgba",std::ios::binary);
    auto sw=read32(secret),sh=read32(secret);read32(secret);read32(secret);
    if(secret && sw>0 && sh>0 && sw<=2048 && sh<=512){
        std::vector<unsigned char> px(size_t(sw)*sh*4);secret.read(reinterpret_cast<char*>(px.data()),px.size());
        if(secret){glGenTextures(1,&secretTexture);glBindTexture(GL_TEXTURE_2D,secretTexture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,sw,sh,0,GL_RGBA,GL_UNSIGNED_BYTE,px.data());glBindTexture(GL_TEXTURE_2D,0);}
    }

    std::ifstream github(directory+"/github-mark.rgba",std::ios::binary);
    auto gw=read32(github),gh=read32(github);read32(github);read32(github);
    if(github && gw>0 && gh>0 && gw<=512 && gh<=512){
        std::vector<unsigned char> rgba(size_t(gw)*gh*4);github.read(reinterpret_cast<char*>(rgba.data()),rgba.size());
        if(github){githubAspect=float(gw)/gh;glGenTextures(1,&githubTexture);glBindTexture(GL_TEXTURE_2D,githubTexture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,gw,gh,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());glBindTexture(GL_TEXTURE_2D,0);}
    }

}

void secretMessage(float x,float y,float w,float h){
    if(!secretTexture)return;
    glPushAttrib(GL_ENABLE_BIT|GL_TEXTURE_BIT|GL_CURRENT_BIT);glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,secretTexture);glColor4f(1,1,1,1);
    glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(x,y);glTexCoord2f(1,0);glVertex2f(x+w,y);glTexCoord2f(1,1);glVertex2f(x+w,y+h);glTexCoord2f(0,1);glVertex2f(x,y+h);glEnd();glPopAttrib();
}
void githubMark(float x,float y,float size) {
    if(!githubTexture)return;
    float w=size,h=size;if(githubAspect>1)h/=githubAspect;else w*=githubAspect;
    glPushAttrib(GL_ENABLE_BIT|GL_TEXTURE_BIT|GL_CURRENT_BIT);glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,githubTexture);glColor4f(1,1,1,1);
    glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(x,y);glTexCoord2f(1,0);glVertex2f(x+w,y);glTexCoord2f(1,1);glVertex2f(x+w,y+h);glTexCoord2f(0,1);glVertex2f(x,y+h);glEnd();glPopAttrib();
}

} // namespace uiart
