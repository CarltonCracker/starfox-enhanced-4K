// Bomb disks and revival/death fades operate on the replaced environment,
// not on the discarded cartridge RGB. Keep OBJ/HUD exclusion independent.
static void check_environment_overlays(starfox::render::SdlGpuEffects& gpu,SDL_GPUDevice* device) {
    using namespace starfox::render;
    unsigned cases=0;
    for(unsigned scale:{1U,2U,4U}) for(unsigned effect=0;effect<7;++effect) {
        Framebuffer frame(24,16,scale);frame.enable_layer_tags(true);
        std::vector<std::uint8_t> input(frame.pixels().size()*4,255);
        for(unsigned y=0;y<frame.stored_height();++y) for(unsigned x=0;x<frame.stored_width();++x) {
            const auto i=std::size_t(y)*frame.stored_width()+x;
            const bool hud=x>=20*scale && y<3*scale;
            const bool model=x>=9*scale && x<13*scale && y>=6*scale && y<10*scale;
            frame.pixels()[i]=hud?129:model?112:y<8*scale?1:2;
            frame.layer_tags()[i]=unsigned(hud?PixelLayer::two_d:model?PixelLayer::three_d:PixelLayer::background);
            for(unsigned c=0;c<3;++c) input[i*4+c]=hud?220:model?180:20;
        }
        BackdropImage sky;sky.width=32;sky.height=16;sky.pixels.assign(512,0xffa07850);
        GpuEffectSettings settings;auto& e=settings.environment;
        e.modes={1,0,1,0};e.motion[0]=8;e.plane={0,1,0,1};e.backdrop=&sky;
        e.classes[1]=6;e.classes[2]=1;
        e.ground_gradient={{{40,80,120,1},{40,80,120,1}}};
        auto expected=input;apply_environment(e,frame,expected);
        if(expected==input) throw std::runtime_error("Environment overlay fixture did not replace scenery");
        const auto environment=expected;
        if(effect<2) {
            settings.circle=GpuEffectSettings::Circle{int(12*scale),int(5*scale),int(11*scale),
                int(scale),0,int(23*scale),int(16*scale),0,31,31,effect==1,effect==1,false};
        } else if(effect<6) {
            settings.colour_math=GpuEffectSettings::ColourMath{255,255,255,effect!=2,effect==4,effect==5};
        } else settings.background_subtract=31;
        std::array<unsigned,3> changed{}; // Sky, ground, model must all be exercised.
        for(unsigned y=0;y<frame.stored_height();++y) for(unsigned x=0;x<frame.stored_width();++x) {
            const auto i=std::size_t(y)*frame.stored_width()+x;
            bool affected=frame.pixels()[i]<128 || effect==5;
            if(settings.circle) {
                const auto& disk=*settings.circle;
                const auto dx=std::int64_t(x)-disk.x,dy=std::int64_t(y)-disk.y;
                affected=affected && int(x)>=disk.left && int(x)<disk.right
                    && int(y)>=disk.top && int(y)<disk.bottom && dx*dx+dy*dy<=std::int64_t(disk.radius)*disk.radius;
            }
            if(!affected) continue;
            for(unsigned c=0;c<3;++c) {
                int value=expected[i*4+c];
                if(settings.circle) {
                    value=(value*31+127)/255;
                    const int fixed=c?31:0;
                    value=effect==1?(value-fixed)/2:value+fixed;
                    value=std::clamp(value,0,31);value=(value<<3)|(value>>2);
                } else if(settings.colour_math) {
                    value=effect==2?value+255:value-255;
                    if(effect==4) value/=2;
                    value=std::clamp(value,0,255);
                } else value=0;
                expected[i*4+c]=std::uint8_t(value);
            }
            if(expected[i*4]!=environment[i*4] || expected[i*4+1]!=environment[i*4+1] || expected[i*4+2]!=environment[i*4+2])
                ++changed[frame.pixels()[i]==1?0:frame.pixels()[i]==2?1:2];
        }
        if(!changed[0] || !changed[1] || !changed[2]) throw std::runtime_error("Environment overlay fixture missed a receiver");
        auto actual=input;
        if(!gpu.apply(device,frame,actual,settings)) throw std::runtime_error(gpu.status());
        if(actual!=expected) throw std::runtime_error("Enhanced-scenery flash/fade order mismatch: effect="
            +std::to_string(effect)+" scale="+std::to_string(scale));
        ++cases;
    }
    std::cout<<"Enhanced environment: "<<cases<<" bomb, death/revival fade and OBJ/HUD protection cases passed\n";
}
