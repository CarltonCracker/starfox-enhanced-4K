#pragma once
#include "starfox/render/backdrop_image.hpp"

namespace starfox::render {
// Panorama, top-down surface, optional moons: one immutable upload. Reuse the
// existing orbital surface masters rather than extruding a panorama edge.
class OrbitalBackdropAtlas {
    BackdropImage image_;
    std::uint64_t panorama_key_{},surface_key_{};
    bool moons_{};
public:
    const BackdropImage& image(const BackdropImage& panorama,const BackdropImage& surface,bool moons=false) {
        if(image_.immutable_upload_key && panorama_key_ && surface_key_ && panorama_key_==panorama.immutable_upload_key
            && surface_key_==surface.immutable_upload_key && moons_==moons) return image_;
        if(!panorama.width || !panorama.height || !surface.width || !surface.height
            || panorama.pixels.size()!=std::size_t(panorama.width)*panorama.height
            || surface.pixels.size()!=std::size_t(surface.width)*surface.height || (moons && panorama.height%2))
            throw std::invalid_argument("Invalid orbital backdrop masters");
        const unsigned rows=panorama.height/(moons?2:1);
        image_.immutable_upload_key=0;image_.width=panorama.width;image_.height=rows*3;
        image_.pixels.resize(std::size_t(image_.width)*image_.height);
        const auto count=std::size_t(image_.width)*rows;
        std::copy_n(panorama.pixels.begin(),count,image_.pixels.begin());
        for(unsigned y=0;y<rows;++y) for(unsigned x=0;x<image_.width;++x) {
            const auto colour=surface.sample_projected(float(x)/std::max(1U,image_.width-1),
                float(y)/std::max(1U,rows-1),3);
            std::uint32_t pixel=0xff000000;
            for(unsigned c=0;c<3;++c) pixel|=std::uint32_t(colour[c]+.5f)<<(8*c);
            image_.pixels[count+std::size_t(y)*image_.width+x]=pixel;
        }
        if(moons) std::copy_n(panorama.pixels.begin()+count,count,image_.pixels.begin()+count*2);
        else std::fill(image_.pixels.begin()+count*2,image_.pixels.end(),0xff000000);
        panorama_key_=panorama.immutable_upload_key;surface_key_=surface.immutable_upload_key;moons_=moons;
        image_.seal_for_upload();return image_;
    }
};
}
