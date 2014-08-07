/**
 * @file Materialway.h
 *
 * @date 31/mar/2014
 * @author Stefano Martina
 */

#ifndef MATERIALWAY_H_
#define MATERIALWAY_H_

#include <map>
#include <vector>
#include <utility>
#include <set>
#include <string>
#include "MaterialObject.h"
//#include "global_constants.h"

class DetectorModule;
class Tracker;
class Barrel;
class Endcap;
class Visitable;
class Disk;
class Layer;

namespace insur {
  class InactiveSurfaces;
  class InactiveTube;
  class InactiveRing;
  class InactiveElement;
  class MatCalc;
}

using insur::InactiveSurfaces;
using insur::InactiveTube;
using insur::InactiveRing;
using insur::InactiveElement;
using insur::MatCalc;


namespace material {

  class MaterialObject;
  class ConversionStation;

  /**
   * @class Materialway
   * @brief Represents a track where the materials are
   * routed from the modules to the appropriate end
   *
   * The materialway is make up from single elements (Element),
   * every element point to the possible single next element.
   * Every module is linked to a materialway element where
   * it puts its materials to be routed, the material is then deposited
   * on the element and forwarded to the next element, passing through the
   * chain from an element to the subsequent one until the material reach
   * its designated end.
   * Every materialway element can perform some transformation on the routed
   * material before forward it to the next element.
   */
  class Materialway {
  private:
    enum Direction { HORIZONTAL, VERTICAL };

    class Section;
    class Station;      //because Train need to use Station and Section, and Station and Section need to use Train

    typedef std::map<const Layer*, std::pair<std::vector<Section*>, Section*> > LayerRodSectionsMap;
    typedef std::map<const Disk*, std::pair<std::vector<Section*>, Section*> > DiskRodSectionsMap;

  public:
    class Train {
    public:
      enum UnitType { GRAMS, GRAMS_METERS, MILLIMITERS };

      Train();
      virtual ~Train();

      void relaseMaterial(Section* section) const;
      void addWagon(std::string massName, double massQuantity, UnitType massUnit);
    private:
      struct Wagon {
        std::string material;
        double droppingGramsMeter;
        Wagon(std::string newMassName, double newDroppingGramsMeter) :
          material(newMassName),
          droppingGramsMeter(newDroppingGramsMeter) {}
      };
      Station* destination;
      std::vector<Wagon> wagons;
    };

  private:

    /**
     * @class Section
     * @brief Represents a single element of the materialway
     */
    class Section {
    public:
      Section(int minZ, int minR, int maxZ, int maxR, Direction bearing, Section* nextSection, bool debug);
      Section(int minZ, int minR, int maxZ, int maxR, Direction bearing, Section* nextSection);
      Section(int minZ, int minR, int maxZ, int maxR, Direction bearing);
      virtual ~Section();

      int isHit(int z, int r, int end, Direction aDirection) const;
      void minZ(int minZ);
      void minR(int minR);
      void maxZ(int maxZ);
      void maxR(int maxR);
      void bearing(Direction bearing);
      void nextSection(Section* nextSection);
      int minZ() const;
      int minR() const;
      int maxZ() const;
      int maxR() const;
      int lenght() const;
      Direction bearing() const;
      Section* nextSection() const;
      bool hasNextSection() const;
      MaterialObject& materialObject();
      void inactiveElement(InactiveElement* inactiveElement);
      InactiveElement* inactiveElement() const;
      //Section* appendNewSection

      virtual void route(const Train& train);
      virtual void getServicesAndPass(const MaterialObject& source);

      bool debug_;
    private:
      int minZ_, minR_, maxZ_, maxR_;
      Section* nextSection_;
      Direction bearing_;
      MaterialObject materialObject_;
      InactiveElement* inactiveElement_; /**< The InactiveElement for hooking up to the existing infrastructure */
    }; //class Section

    class Station : public Section {
    public:
      Station(int minZ, int minR, int maxZ, int maxR, Direction bearing, ConversionStation& conversionStation, Section* nextSection);
      Station(int minZ, int minR, int maxZ, int maxR, Direction bearing, ConversionStation& conversionStation);
      virtual ~Station();

      enum Type { LAYER, TERMINUS };
      struct ConversionRule {

      };

      virtual void route(const Train& train);
      virtual void getServicesAndPass(const MaterialObject& source);

      ConversionStation& conversionStation();

    private:
      size_t labelHash;
      ConversionStation& conversionStation_;
    };

    typedef std::vector<Section*> SectionVector;
    typedef std::vector<Station*> StationVector;
    typedef std::map<const DetectorModule*, Section*> ModuleSectionMap;

    /**
     * @class Boundary
     * @brief Represents a boundary where the services are routed around
     */
    class Boundary {
    public:
      Boundary();
      Boundary(const Visitable* containedElement, int minZ, int minR, int maxZ, int maxR);
      virtual ~Boundary();

      int isHit(int z, int r, Direction aDirection) const;

      void minZ(int minZ);
      void minR(int minR);
      void maxZ(int maxZ);
      void maxR(int maxR);
      void outgoingSection(Section* outgoingSection);
      int minZ() const;
      int minR() const;
      int maxZ() const;
      int maxR() const;
      Section* outgoingSection();
    private:
      int minZ_, minR_, maxZ_, maxR_;
      Section* outgoingSection_;
      const Visitable* containedElement_;
    }; //class Boundary

    typedef std::map<const Barrel*, Boundary*> BarrelBoundaryMap;
    typedef std::map<const Endcap*, Boundary*> EndcapBoundaryMap;

    struct BoundaryComparator {
      bool operator()(Boundary* const& one, Boundary* const& two) {
        return ((one->maxZ() > two->maxZ()) || (one->maxR() > two->maxR()));
      }
    };

    typedef std::set<Boundary*, BoundaryComparator> BoundariesSet;

    /**
     * @class OuterUsher
     * @brief Is the core of the functionality that builds sections across boundaries
     * starting from a point and ending to another section or to the upper right angle
     */
    class OuterUsher {
     public:
      OuterUsher(SectionVector& sectionsList, BoundariesSet& boundariesList);
      virtual ~OuterUsher();

      void go(Boundary* boundary, const Tracker& tracker, Direction direction);         /**< start the process of section building, returns pointer to the first */
    private:
      SectionVector& sectionsList_;
      BoundariesSet& boundariesList_;

      bool findBoundaryCollision(int& collision, int& border, int startZ, int startR, const Tracker& tracker, Direction direction);
      bool findSectionCollision(std::pair<int,Section*>& sectionCollision, int startZ, int startR, int end, Direction direction);
      bool buildSection(Section*& firstSection, Section*& lastSection, int& startZ, int& startR, int end, Direction direction);
      bool buildSectionPair(Section*& firstSection, Section*& lastSection, int& startZ, int& startR, int collision, int border, Direction direction);
      Section* splitSection(Section* section, int collision, Direction direction);
      Direction inverseDirection(Direction direction) const;
      void updateLastSectionPointer(Section* lastSection, Section* newSection);
    }; //class OuterUsher

    /**
     * @class InnerUsher
     * @brief Build the internal sections of a boundary
     */
    class InnerUsher {
    public:
      InnerUsher(
          SectionVector& sectionsList,
          StationVector& stationListFirst,
          BarrelBoundaryMap& barrelBoundaryAssociations,
          EndcapBoundaryMap& endcapBoundaryAssociations,
          ModuleSectionMap& moduleSectionAssociations,
          LayerRodSectionsMap& layerRodSections,
          DiskRodSectionsMap& diskRodSections);
      virtual ~InnerUsher();

      void go(Tracker& tracker);
    private:
      SectionVector& sectionsList_;
      StationVector& stationListFirst_;
      BarrelBoundaryMap& barrelBoundaryAssociations_;
      EndcapBoundaryMap& endcapBoundaryAssociations_;
      ModuleSectionMap& moduleSectionAssociations_;
      LayerRodSectionsMap& layerRodSections_;
      DiskRodSectionsMap& diskRodSections_;
    }; //class InnerUsher

  public:
    Materialway();
    virtual ~Materialway();

    bool build(Tracker& tracker, InactiveSurfaces& inactiveSurface, MatCalc& materialCalc);

  private:
    BoundariesSet boundariesList_;       /**< Vector for storing all the boundaries */
    SectionVector sectionsList_;         /**< Vector for storing all the sections (also stations)*/
    StationVector stationListFirst_;         /**< Pointers to first step stations*/

    OuterUsher outerUsher;
    InnerUsher innerUsher;

    static const double gridFactor;                                     /**< the conversion factor for using integers in the algorithm (helps finding collisions),
                                                                            actually transforms millimiters in microns */
    static const int sectionWidth;     /**< the width of a section */
    static const int safetySpace;           /**< the safety space between sections */
    //static const double globalMaxZ_mm;                     /**< the Z coordinate of the end point of the sections */
    //static const double globalMaxR_mm;                   /**< the rho coordinate of the end point of the sections */
    //static const int globalMaxZ;
    //static const int globalMaxR;
    static const int boundaryPadding;
    static const int boundaryPrincipalPadding;
    static const int globalMaxZPadding;
    static const int globalMaxRPadding;
    static const int layerSectionMargin;
    static const int diskSectionMargin;
    static const int layerSectionRightMargin;
    static const int diskSectionUpMargin;
    static const int sectionTolerance;
    static const int layerStationLenght;
    static const int layerStationWidth;

    static int discretize(double input);
    static double undiscretize(int input);

    bool buildBoundaries(const Tracker& tracker);             /**< build the boundaries around barrels and endcaps */
    void buildExternalSections(const Tracker& tracker);       /**< build the sections outside the boundaries */
    void buildInternalSections(Tracker& tracker);                             /**< build the sections inside the boundaries */
    void buildInactiveElements();
    void routeModuleServices();
    void routeRodMaterials();
    void firstStepConversions();
    void populateInactiveElements();
    void testTrains();
    void buildInactiveSurface(InactiveSurfaces& inactiveSurface);
    InactiveElement* buildOppositeInactiveElement(InactiveElement* inactiveElement);


    BarrelBoundaryMap barrelBoundaryAssociations_;
    EndcapBoundaryMap endcapBoundaryAssociations_;
    ModuleSectionMap moduleSectionAssociations_; /**< Map that associate each module with the section that it feeds */
    LayerRodSectionsMap layerRodSections_;      /**< maps for sections of the rods */
    DiskRodSectionsMap diskRodSections_;
    //std::map<Boundary&, Section*> boundarySectionAssociations;         /**< Map that associate each boundary with the outgoing section (for the construction) */
  };

} /* namespace material */

#endif /* MATERIALWAY_H_ */
