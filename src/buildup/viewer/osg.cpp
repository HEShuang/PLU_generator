#include "buildup/viewer/osg.hpp"

#include <osg/ref_ptr>
#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osgViewer/Viewer>
#include <osg/LightModel>
#include <osg/Light>
#include <osg/LightSource>

#include <osg/LineWidth>

//#include <osgText/Text>
#include <osgUtil/Tessellator>
#include <osgUtil/Optimizer>
#include <gdal/ogrsf_frmts.h>

#define WHITE osg::Vec4f(1.0f,1.0f,1.0f,0.2f)
#define BLACK osg::Vec4f(0.0f,0.0f,0.0f,1.0f)
#define RED osg::Vec4f(1.0f,0.0f,0.0f,1.0f)
#define GREEN osg::Vec4f(0.0f,1.0f,0.0f,1.0f)
#define BLUE osg::Vec4f(0.0f,0.0f,1.0f,1.0f)
#define AZURE2 osg::Vec4f(0.88f,0.93f,0.93f,1.0f)
#define SKYBLUE3 osg::Vec4f(0.42f,0.65f,0.80f,1.0f)
#define WIN osg::Vec4f(0.55f,0.66f,0.44f,0.5f)
#define DOOR osg::Vec4f(0.46f,0.46f,0.46f,1.0f)
#define ROOF osg::Vec4f(0.78f,0.36f,0.30f,1.0f)
#define WALL osg::Vec4f(0.80,0.31f,0.36f,1.0f)
namespace io{
    //noninterface func
    class UseEventHandler:public osgGA::GUIEventHandler
    {
    public:
        virtual bool handle(const osgGA::GUIEventAdapter& ea,osgGA::GUIActionAdapter& aa)
        {
            osgViewer::Viewer* viewer = dynamic_cast<osgViewer::Viewer*>(&aa);
            if(!viewer)return false;

            if (ea.getEventType()==osgGA::GUIEventAdapter::KEYDOWN)
            {

                int n = viewer->getSceneData()->asGroup()->getNumChildren()-1;  // number of layers (footprints/lod models)

                if(ea.getKey()==osgGA::GUIEventAdapter::KEY_Up)
                    for(int i= 0; i<n; i++)
                        if(viewer->getSceneData()->asGroup()->getChild(i)->getNodeMask()&& i+1<n)
                        {
                            viewer->getSceneData()->asGroup()->getChild(i+1)->setNodeMask(1);
                            viewer->getSceneData()->asGroup()->getChild(i)->setNodeMask(0);
                            break;

                        }

                if(ea.getKey()==osgGA::GUIEventAdapter::KEY_Down)
                    for(int i=n-1; i>=0; i--)
                        if(viewer->getSceneData()->asGroup()->getChild(i)->getNodeMask() && i-1>=0)
                        {
                            viewer->getSceneData()->asGroup()->getChild(i)->setNodeMask(0);
                            viewer->getSceneData()->asGroup()->getChild(i-1)->setNodeMask(1);
                            break;

                        }

            }
            return false;
        }
    };


    int viewNode(osg::ref_ptr<osg::Node> node)
    {

        osg::DisplaySettings::instance()->setNumMultiSamples(8);

        osgViewer::Viewer viewer;
        viewer.setLightingMode(osg::View::NO_LIGHT);

        viewer.setUpViewInWindow(0,0,600,400);
        viewer.setCameraManipulator(0);
        viewer.getCamera()->setClearColor(AZURE2);
        viewer.setSceneData(node.get());
        viewer.addEventHandler(new UseEventHandler());
        viewer.realize();
        viewer.run();
        return 0;
    }


    osg::ref_ptr<osg::Geode> makeGeode(OGRPolygon* polygon, osg::PrimitiveSet::Mode mode,bool doTessellate, bool blendOn, osg::Vec4 color)
    {
        if(!polygon || polygon->IsEmpty())
            return 0;
        osg::ref_ptr<osg::Geode> geode = new osg::Geode();
        osg::Geometry *geom = new osg::Geometry();
        osg::Vec3Array *verts = new osg::Vec3Array();
        geom->setVertexArray(verts);

        OGRLinearRing *ring = polygon->getExteriorRing();

        int n_pts = ring->getNumPoints()-1;

        for(int j=n_pts-1; j>-1; j--)
        {
            OGRPoint pt;
            ring->getPoint(j,&pt);
            verts->push_back(osg::Vec3(pt.getX(),pt.getY(),pt.getZ()));
        }
        int n_start = 0;
        geom->addPrimitiveSet(new osg::DrawArrays(mode,n_start,n_pts));
        n_start += n_pts;

        if(int n_rings_inter = polygon->getNumInteriorRings())
        {
            for(int j=0; j<n_rings_inter; j++)
            {
                OGRLinearRing *ring_j = polygon->getInteriorRing(j);

                int n_pts_j = ring_j->getNumPoints()-1;

                for(int k=n_pts_j-1; k>-1; k--)
                {
                    OGRPoint pt;
                    ring_j->getPoint(k,&pt);
                    verts->push_back(osg::Vec3(pt.getX(),pt.getY(),pt.getZ()));
                }
                geom->addPrimitiveSet(new osg::DrawArrays(mode,n_start,n_pts_j));
                n_start += n_pts_j;

            }
        }

        if(mode==osg::PrimitiveSet::POLYGON && doTessellate)
        {
            osg::ref_ptr<osgUtil::Tessellator> tess = new osgUtil::Tessellator;
            tess->setTessellationType(osgUtil::Tessellator::TESS_TYPE_GEOMETRY);
            tess->setBoundaryOnly(false);
            tess->setWindingType(osgUtil::Tessellator::TESS_WINDING_ODD);
            tess->retessellatePolygons(*geom);
        }


        if(blendOn)
        {
            osg::StateSet* ss = geode->getOrCreateStateSet();
            ss->setMode(GL_BLEND,osg::StateAttribute::ON);
        }

        osg::Vec4Array* colors = new osg::Vec4Array();
        colors->push_back(color);
        geom->setColorArray(colors);
        geom->setColorBinding(osg::Geometry::BIND_OVERALL);
        geom->getTexCoordArrayList().clear();

        geode->addDrawable(geom);

        osg::LineWidth* width = new osg::LineWidth;
        width->setWidth(1.3);
        geode->getOrCreateStateSet()->setAttributeAndModes(width,osg::StateAttribute::ON);



        return geode;
    }


    //interface func//
//    void display(OGRPolygon* ply)
//    {
//        viewNode(makeGeode(ply,osg::PrimitiveSet::Mode::LINE_LOOP,false,WIN));
//    }
//
//    void display(OGRPolygon* ply1, OGRPolygon* ply2)
//    {
//        osg::ref_ptr<osg::Group> root = new osg::Group();
//        root->addChild(makeGeode(ply1,osg::PrimitiveSet::Mode::LINE_LOOP,false,BLACK));
//        root->addChild(makeGeode(ply2,osg::PrimitiveSet::Mode::LINE_LOOP,false,RED));
//        viewNode(root);
//    }

    void display(std::vector<Building>& bldgs,std::map<int,Lot>& lots)
    {
        osg::ref_ptr<osg::Group> root = new osg::Group();

        for(size_t i=0; i<bldgs.size(); ++i)
        {
            OGRMultiPolygon* plys = bldgs[i].extrude_box();
            osg::ref_ptr<osg::Group> wire = new osg::Group();
            osg::ref_ptr<osg::Group> surface = new osg::Group();
            for(int j=0; j<plys->getNumGeometries(); ++j)
            {
                wire->addChild(makeGeode((OGRPolygon*)(plys->getGeometryRef(j)),osg::PrimitiveSet::Mode::LINE_STRIP,false,false,BLACK));
                surface->addChild(makeGeode((OGRPolygon*)(plys->getGeometryRef(j)),osg::PrimitiveSet::Mode::POLYGON,false,false,SKYBLUE3));

            }
            root->addChild(wire);
            root->addChild(surface);
        }

        osg::ref_ptr<osg::Group> gpLots = new osg::Group();
        std::map<int,Lot>::iterator it;
        for(it=lots.begin(); it!=lots.end(); ++it)
        {
            OGRPolygon *ply = (OGRPolygon*)(it->second.polygon()->Buffer(-0.1));
            gpLots->addChild(makeGeode(ply,osg::PrimitiveSet::Mode::POLYGON,true,false,WHITE));
            ply->empty();
            gpLots->addChild(makeGeode(it->second.polygon(),osg::PrimitiveSet::Mode::LINE_LOOP,false,false,BLACK));
        }
        root->addChild(gpLots);


        osg::DisplaySettings::instance()->setNumMultiSamples(8);

        osgViewer::Viewer viewer;
        viewer.setLightingMode(osg::View::NO_LIGHT);

        viewer.setUpViewInWindow(0,0,600,400);
        viewer.setCameraManipulator(0);
        viewer.getCamera()->setClearColor(AZURE2);
        viewer.setSceneData(root.get());
       // viewer.addEventHandler(new UseEventHandler());
        viewer.realize();
        viewer.run();
    }

    void display(std::map<int,std::vector<Building> >& exp_bldgs, std::map<int,Lot>& lots)
    {
        osg::ref_ptr<osg::Group> root = new osg::Group();

        {
            std::map<int,std::vector<Building> >::iterator it;
            for(it=exp_bldgs.begin(); it!=exp_bldgs.end(); ++it)
            {
                osg::ref_ptr<osg::Group> exp = new osg::Group();
                std::vector<Building>& bldgs = it->second;
                for(size_t i=0; i<bldgs.size(); ++i)
                {
                    OGRMultiPolygon* plys = bldgs[i].extrude_box();
                    osg::ref_ptr<osg::Group> wire = new osg::Group();
                    osg::ref_ptr<osg::Group> surface = new osg::Group();
                    for(int j=0; j<plys->getNumGeometries(); ++j)
                    {
                        wire->addChild(makeGeode((OGRPolygon*)(plys->getGeometryRef(j)),osg::PrimitiveSet::Mode::LINE_STRIP,false,false,BLACK));
                        surface->addChild(makeGeode((OGRPolygon*)(plys->getGeometryRef(j)),osg::PrimitiveSet::Mode::POLYGON,false,false,SKYBLUE3));

                    }
                    exp->addChild(wire);
                    exp->addChild(surface);

                }
                exp->setNodeMask(0);
                root->addChild(exp);
            }
            root->getChild(0)->setNodeMask(1);
        }

        {
            osg::ref_ptr<osg::Group> gpLots = new osg::Group();
            std::map<int,Lot>::iterator it;
            for(it=lots.begin(); it!=lots.end(); ++it)
            {
                OGRPolygon *ply = (OGRPolygon*)(it->second.polygon()->Buffer(-0.1));
                gpLots->addChild(makeGeode(ply,osg::PrimitiveSet::Mode::POLYGON,true,false,WHITE));
                ply->empty();
                gpLots->addChild(makeGeode(it->second.polygon(),osg::PrimitiveSet::Mode::LINE_LOOP,false,false,BLACK));
            }
            root->addChild(gpLots);
        }
        viewNode(root);
    }

    void display_lod3(std::map<int,std::vector<Building> >& exp_bldgs, std::map<int,Lot>& lots)
    {
        osg::ref_ptr<osg::Group> root = new osg::Group();

        {
            std::map<int,std::vector<Building> >::iterator it;
            for(it=exp_bldgs.begin(); it!=exp_bldgs.end(); ++it)
            {
                osg::ref_ptr<osg::Group> exp = new osg::Group();
                std::vector<Building>& bldgs = it->second;
                for(size_t i=0; i<bldgs.size(); ++i)
                {
                    Surface* tree = bldgs[i].extrude_lod3(3.0,13.0,4.0);
                    std::vector<Surface*> lod3;
                    tree->getLeaves(lod3);

                    osg::ref_ptr<osg::Group> wire = new osg::Group();
                    osg::ref_ptr<osg::Group> surface = new osg::Group();
                    for(size_t j=0;j<lod3.size();++j)
                    {

                        SurfaceType type = lod3[j]->getType();
                        switch (type)
                        {
                        case SurfaceType::Roof:
                            surface->addChild(makeGeode(lod3[j]->getGeom(),osg::PrimitiveSet::Mode::POLYGON,false,false,ROOF));
                            wire->addChild(makeGeode(lod3[j]->getGeom(),osg::PrimitiveSet::Mode::LINE_LOOP,false,false,BLACK));
                            break;

                        case SurfaceType::Window:
                            surface->addChild(makeGeode(lod3[j]->getGeom(),osg::PrimitiveSet::Mode::POLYGON,false,true,WIN));
                            wire->addChild(makeGeode(lod3[j]->getGeom(),osg::PrimitiveSet::Mode::LINE_LOOP,false,false,BLACK));
                            break;

                        case SurfaceType::Door:
                            surface->addChild(makeGeode(lod3[j]->getGeom(),osg::PrimitiveSet::Mode::POLYGON,false,true,DOOR));
                            wire->addChild(makeGeode(lod3[j]->getGeom(),osg::PrimitiveSet::Mode::LINE_LOOP,false,false,BLACK));
                            break;


                        default:
                            surface->addChild(makeGeode(lod3[j]->getGeom(),osg::PrimitiveSet::Mode::POLYGON,true,false,WALL));
                            //wire->addChild(makeGeode(lod3[j]->getGeom(),osg::PrimitiveSet::Mode::LINE_LOOP,false,false,BLACK));
                            break;
                        }
                    }

                    std::vector<Surface*> floors;
                    tree->getSurfaceByType(SurfaceType::Floor,floors);
                    for(size_t j=0;j<floors.size();++j)
                        wire->addChild(makeGeode(floors[j]->getGeom(),osg::PrimitiveSet::Mode::LINE_LOOP,false,false,BLACK));


                    exp->addChild(wire);
                    exp->addChild(surface);
                    delete tree;

                }
                exp->setNodeMask(0);
                root->addChild(exp);
            }
            root->getChild(0)->setNodeMask(1);
        }

        {
            osg::ref_ptr<osg::Group> gpLots = new osg::Group();
            std::map<int,Lot>::iterator it;
            for(it=lots.begin(); it!=lots.end(); ++it)
            {
                OGRPolygon *ply = (OGRPolygon*)(it->second.polygon()->Buffer(-0.3));
                gpLots->addChild(makeGeode(ply,osg::PrimitiveSet::Mode::POLYGON,true,false,WHITE));
                ply->empty();
                gpLots->addChild(makeGeode(it->second.polygon(),osg::PrimitiveSet::Mode::LINE_LOOP,false,false,BLACK));
            }
            root->addChild(gpLots);
        }
        viewNode(root);
    }


//    //for test
//     osg::ref_ptr<osg::Geode> makeGeode(double x[4],double y[4],const char* str)
//     {
//
//        osg::Vec3Array *verts = new osg::Vec3Array();
//        for(int i=0;i<4;++i)
//            verts->push_back(osg::Vec3(x[i],y[i],0));
//
//        osg::Geometry *geom = new osg::Geometry();
//        geom->setVertexArray(verts);
//        geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::Mode::LINE_LOOP,0,4));

//        osg::Vec4Array* colors = new osg::Vec4Array();
//        colors->push_back(BLACK);
//        colors->push_back(RED);
//        colors->push_back(GREEN);
//        colors->push_back(BLUE);
//        geom->setColorArray(colors);
//        geom->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
//
//        osg::ref_ptr<osg::Geode> geode = new osg::Geode();
//        geode->addDrawable(geom);
//
//        osg::ref_ptr<osgText::Text> text = new osgText::Text;
//        text->setPosition(verts->at(0));
//        text->setAxisAlignment(osgText::Text::XY_PLANE);
//        text->setText(str);
//        text->setCharacterSize(1,1);
//        text->setColor(osg::Vec4(0.0f,0.0f,0.0f,1.0f));
//        geode->addDrawable(text);
//
//        return geode;
//     }
//
//
//    void display(double x1[4],double y1[4],double x2[4],double y2[4])
//    {
//        osg::ref_ptr<osg::Group> root = new osg::Group();
//        root->addChild(makeGeode(x1,y1,"b1"));
//        root->addChild(makeGeode(x2,y2,"b2"));
//        viewNode(root);
//    }
} //namespace io
