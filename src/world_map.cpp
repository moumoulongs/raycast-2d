#include "world_map.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

char WorldTexture::sample(double u,double v) const {
    auto index=[](double p,size_t n){const auto i = static_cast<size_t>((p-std::floor(p))*n); return i < n ? i : size_t{0};};
    return pixels[index(v,height)*width + index(u,width)];
}
void WorldMap::animate(double seconds) {
    for (const auto& lift : lifts_) {
        const double phase = std::fmod(seconds/lift.period + lift.offset, 1.0);
        // Hold at both landings, easing in and out between them.
        const double t = phase < .25 ? 0 : phase < .5 ? (phase-.25)*4 : phase < .75 ? 1 : (1-phase)*4;
        legend_.at(lift.symbol).floor = lift.low + (lift.high-lift.low)*t*t*(3-2*t);
    }
}
bool WorldMap::contains(int x,int y) const {
    return x>=0 && y>=0 && size_t(y)<grid_.size() && size_t(x)<grid_[y].size();
}
WorldMap::WorldMap(const std::filesystem::path& mapFile,const std::filesystem::path& textureFile) {
    std::ifstream file(mapFile), atlas(textureFile);
    *this = WorldMap(file, atlas, mapFile, textureFile);
}
WorldMap::WorldMap(std::istream& file,std::istream& atlas,const std::filesystem::path& mapFile,const std::filesystem::path& textureFile) {
    std::string line;
    auto bad=[&](const auto& path){ throw std::runtime_error("Invalid or missing resource: "+path.string()+" near: "+line); };
    if(!atlas) bad(textureFile);
    while(std::getline(atlas,line)) {
        if(line.empty() || line[0]==';') continue;
        std::istringstream header(line); int id,color,w,h;
        if(!(header>>id>>color>>w>>h)||id<=0||textures_.count(id)||color<0||color>15||w<1||h<1||w>256||h>256) bad(textureFile);
        WorldTexture tex{{}, static_cast<std::uint16_t>(w), static_cast<std::uint16_t>(h), static_cast<unsigned char>(color)};
        std::string effect;
        if (header >> effect) {
            if (effect != "flow" || !(header >> tex.flowX >> tex.flowY) || !std::isfinite(tex.flowX) || !std::isfinite(tex.flowY) || std::abs(tex.flowX)>8 || std::abs(tex.flowY)>8) bad(textureFile);
        }
        tex.pixels.reserve(w*h);
        while(h--){if(!std::getline(atlas,line)) bad(textureFile); if(!line.empty()&&line.back()=='\r') line.pop_back(); if(int(line.size())!=w) bad(textureFile); tex.pixels += line;}
        textures_.emplace(id,std::move(tex));
    }
    bool spawn=false;
    if(!file) bad(mapFile);
    while(std::getline(file,line)) {
        std::istringstream row(line); std::string op; if(!(row>>op)||op[0]==';')continue;
        if(op=="tile"){ char c; Tile t{}; if(!(row>>c>>t.floor>>t.wall>>t.surface)||std::abs(t.floor)>32||!textures_.count(t.surface)||(t.wall&&!textures_.count(t.wall))||!legend_.emplace(c,t).second)bad(mapFile); }
        else if(op=="lift"){Lift l{};if(!(row>>l.symbol>>l.low>>l.high>>l.period>>l.offset)||!legend_.count(l.symbol)||legend_.at(l.symbol).wall||!(l.low>=-32&&l.high<=32&&l.high>l.low&&l.period>=1&&l.period<=300&&l.offset>=0&&l.offset<1))bad(mapFile);lifts_.push_back(l);}
        else if(op=="swarm") {
            Swarm s{};
            if(!(row>>s.x>>s.y>>s.radius>>s.height>>s.count) || !(s.x>=0&&s.x<512&&s.y>=0&&s.y<512&&s.radius>0&&s.radius<=16&&s.height>0&&s.height<=16) || s.count<1 || s.count>128 || swarms_.size()>=16) bad(mapFile);
            swarms_.push_back(s);
        }
        else if(op=="spawn"){if(!(row>>start_.x>>start_.y>>direction_.x>>direction_.y)||direction_.magnitudeSquared()<0.0001)bad(mapFile); spawn=true;}
        else if(op=="prop"){WorldProp p{};if(!(row>>p.textureId>>p.x>>p.y>>p.width>>p.height)||!textures_.count(p.textureId)||p.width<=0||p.height<=0)bad(mapFile);props_.push_back(p);}
        else if(op=="row"){std::string cells;if(!(row>>cells)||cells.size()>512||grid_.size()>=512||(!grid_.empty()&&cells.size()!=grid_[0].size()))bad(mapFile);for(char c:cells)if(!legend_.count(c))bad(mapFile);grid_.push_back(cells);}
        else bad(mapFile);
    }
    auto walkable=[&](double x,double y){return !grid_.empty()&&x>=0&&y>=0&&x<grid_[0].size()&&y<grid_.size()&&!isWall(int(x),int(y));};
    if(!spawn||!walkable(start_.x,start_.y))bad(mapFile);
    for(const auto& p:props_)if(!walkable(p.x,p.y))bad(mapFile);
}
