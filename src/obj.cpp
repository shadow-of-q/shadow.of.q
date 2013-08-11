#include "obj.hpp"
#include "console.hpp"
#include "base/tools.hpp"
#include "base/stl.hpp"
#include "base/map.hpp"

#include <cstring>
#include <cstdlib>
#include <cstdio>

namespace cube {
  // helper to load OBJ wavefront files
  struct objloader {
    static const int FILE_NAME_SZ = 1024;
    static const int MAT_NAME_SZ = 1024;
    static const int LINE_SZ = 4096;
    static const int MAX_VERT_NUM = 4;

    // face as parsed by the OBJ loader
    struct face {
      int vertexID[MAX_VERT_NUM];
      int normalID[MAX_VERT_NUM];
      int textureID[MAX_VERT_NUM];
      int vertexNum;
      int materialID;
    };

    // vector as parsed by the OBJ loader
    struct vec {
      double e[3];
    };

    // material as parsed by the OBJ loader
    struct mat {
      INLINE void setDefault(void);
      char name[MAT_NAME_SZ];
      char map_Ka[FILE_NAME_SZ];
      char map_Kd[FILE_NAME_SZ];
      char map_D[FILE_NAME_SZ];
      char map_Bump[FILE_NAME_SZ];
      double amb[3], diff[3], spec[3];
      double km;
      double reflect;
      double refract;
      double trans;
      double shiny;
      double glossy;
      double refract_index;
    };

    INLINE objloader(void) : objSaved(NULL), mtlSaved(NULL) {}
    bool loadobj(const char *fileName);
    bool loadmtl(const char *mtlFileName, const char *objFileName);
    int findmaterial(const char *name);
    face* parseFace(void);
    vec* parseVector(void);
    void parsevertexIndex(objloader::face &face);
    INLINE int getListIndex(int currMax, int index) {
      if (index == 0) return -1;
      if (index < 0)  return currMax + index;
      return index - 1;
    }
    void getListIndexV(int current_max, int *indices) {
      loopi(MAX_VERT_NUM) indices[i] = this->getListIndex(current_max, indices[i]);
    }
    INLINE face *allocface(void) { return faceAllocator.allocate(); }
    INLINE vec *allocvec(void)   { return vectorAllocator.allocate(); }
    INLINE mat *allocmat(void)   { return matAllocator.allocate(); }
    vector<objloader::vec*>  vertexList;
    vector<objloader::vec*>  normalList;
    vector<objloader::vec*>  textureList;
    vector<objloader::face*> faceList;
    vector<objloader::mat*>  materialList;
    growingpool<objloader::face> faceAllocator;
    growingpool<objloader::vec> vectorAllocator;
    growingpool<objloader::mat> matAllocator;
    char *objSaved, *mtlSaved; // for tokenize
    static const char *whiteSpace;
  };
  const char *objloader::whiteSpace = " \t\n\r";

  void objloader::mat::setDefault(void) {
    this->map_Ka[0] = this->map_Kd[0] = this->map_D[0] = this->map_Bump[0] = '\0';
    this->amb[0]  = this->amb[1]  = this->amb[2]  = 0.2;
    this->diff[0] = this->diff[1] = this->diff[2] = 0.8;
    this->spec[0] = this->spec[1] = this->spec[2] = 1.0;
    this->reflect = 0.0;
    this->trans = 1.;
    this->glossy = 98.;
    this->shiny = 0.;
    this->refract_index = 1.;
  }

  void objloader::parsevertexIndex(objloader::face &face) {
    char **saved = &this->objSaved;
    char *temp_str = NULL, *token = NULL;
    int vertexNum = 0;

    while ((token = tokenize(NULL, whiteSpace, saved)) != NULL) {
      if (face.textureID != NULL) face.textureID[vertexNum] = 0;
      if (face.normalID != NULL)  face.normalID[vertexNum] = 0;
      face.vertexID[vertexNum] = atoi(token);

      if (contains(token, "//")) {
        temp_str = strchr(token, '/');
        temp_str++;
        face.normalID[vertexNum] = atoi(++temp_str);
      } else if (contains(token, "/")) {
        temp_str = strchr(token, '/');
        face.textureID[vertexNum] = atoi(++temp_str);
        if (contains(temp_str, "/")) {
          temp_str = strchr(temp_str, '/');
          face.normalID[vertexNum] = atoi(++temp_str);
        }
      }
      vertexNum++;
    }
    face.vertexNum = vertexNum;
  }

  int objloader::findmaterial(const char *name) {
    loopv(materialList) if (strequal(name, materialList[i]->name)) return i;
    return -1;
  }

  objloader::face *objloader::parseFace(void) {
    auto face = allocface();
    parsevertexIndex(*face);
    getListIndexV(vertexList.size(), face->vertexID);
    getListIndexV(textureList.size(), face->textureID);
    getListIndexV(normalList.size(), face->normalID);
    return face;
  }

  objloader::vec *objloader::parseVector(void) {
    auto v = allocvec();
    v->e[0] = v->e[1] = v->e[2] = 0.;
    loopi(3) {
      auto str = tokenize(NULL, whiteSpace, &this->objSaved);
      if (str == NULL) break;
      v->e[i] = atof(str);
    }
    return v;
  }

  bool objloader::loadmtl(const char *mtlFileName, const char *objFileName)
  {
    char *tok = NULL;
    FILE *mtlFile = NULL;
    objloader::mat *currMat = NULL;
    char currLine[LINE_SZ];
    int lineNumber = 0;
    char material_open = 0;

    // Try to get directly from the provided file name
    mtlFile = fopen(mtlFileName, "r");

    // We were not able to find it
    if (mtlFile == NULL) return false;

    // Parse it
    while (fgets(currLine, LINE_SZ, mtlFile)) {
      auto saved = &this->mtlSaved;
      tok = tokenize(currLine, " \t\n\r", saved);
      lineNumber++;

      if (tok == NULL || strequal(tok, "//") || strequal(tok, "#")) // skip comments
        continue;
      else if (strequal(tok, "newmtl")) { // start material
        material_open = 1;
        currMat = allocmat();
        currMat->setDefault();
        strncpy(currMat->name, tokenize(NULL, whiteSpace, saved), MAT_NAME_SZ);
        materialList.add(currMat);
      } else if (strequal(tok, "Ka") && material_open) { // ambient
        currMat->amb[0] = atof(tokenize(NULL, " \t", saved));
        currMat->amb[1] = atof(tokenize(NULL, " \t", saved));
        currMat->amb[2] = atof(tokenize(NULL, " \t", saved));
      } else if (strequal(tok, "Kd") && material_open) { // diff
        currMat->diff[0] = atof(tokenize(NULL, " \t", saved));
        currMat->diff[1] = atof(tokenize(NULL, " \t", saved));
        currMat->diff[2] = atof(tokenize(NULL, " \t", saved));
      } else if (strequal(tok, "Ks") && material_open) { // specular
       currMat->spec[0] = atof(tokenize(NULL, " \t", saved));
        currMat->spec[1] = atof(tokenize(NULL, " \t", saved));
        currMat->spec[2] = atof(tokenize(NULL, " \t", saved));
      } else if (strequal(tok, "Ns") && material_open) // shiny
        currMat->shiny = atof(tokenize(NULL, " \t", saved));
      else if (strequal(tok, "Km") && material_open) // map_Km
        currMat->km = atof(tokenize(NULL, " \t", saved));
      else if (strequal(tok, "d") && material_open) // transparent
        currMat->trans = atof(tokenize(NULL, " \t", saved));
      else if (strequal(tok, "r") && material_open) // reflection
        currMat->reflect = atof(tokenize(NULL, " \t", saved));
      else if (strequal(tok, "sharpness") && material_open) // glossy
        currMat->glossy = atof(tokenize(NULL, " \t", saved));
      else if (strequal(tok, "Ni") && material_open) // refract index
        currMat->refract_index = atof(tokenize(NULL, " \t", saved));
      else if (strequal(tok, "map_Ka") && material_open) // map_Ka
        strncpy(currMat->map_Ka, tokenize(NULL, " \"\t\r\n", saved), FILE_NAME_SZ);
      else if (strequal(tok, "map_Kd") && material_open) // map_Kd
        strncpy(currMat->map_Kd, tokenize(NULL, " \"\t\r\n", saved), FILE_NAME_SZ);
      else if (strequal(tok, "map_D") && material_open) // map_D
        strncpy(currMat->map_D, tokenize(NULL, " \"\t\r\n", saved), FILE_NAME_SZ);
      else if (strequal(tok, "map_Bump") && material_open) // map_Bump
        strncpy(currMat->map_Bump, tokenize(NULL, " \"\t\r\n", saved), FILE_NAME_SZ);
      else if (strequal(tok, "illum") && material_open) { } // illumination type
      else console::out("objloader: unknown command : %s "
                        "in material file %s at line %i, \"%s\"",
                        tok, mtlFileName, lineNumber, currLine);
    }

    fclose(mtlFile);
    return true;
  }

  bool objloader::loadobj(const char *fileName) {
    FILE *objFile;
    int currmaterial = -1; 
    char *tok = NULL;
    char currLine[LINE_SZ];
    int lineNumber = 0;

    // open scene
    objFile = fopen(fileName, "r");
    if (objFile == NULL) {
      console::out("objloader: error reading file: %s", fileName);
      return false;
    }

    // parser loop
    while (fgets(currLine, LINE_SZ, objFile)) {
      char **saved = &this->objSaved;
      tok = tokenize(currLine, " \t\n\r", saved);
      lineNumber++;

      // skip comments
      if (tok == NULL || tok[0] == '#') continue;
      // parse objects
      else if (strequal(tok, "v")) // vertex
        vertexList.add(this->parseVector());
      else if (strequal(tok, "vn")) // vertex normal
        normalList.add(this->parseVector());
      else if (strequal(tok, "vt")) // vertex texture
        textureList.add(this->parseVector());
      else if (strequal(tok, "f")) { // face
        objloader::face *face = this->parseFace();
        face->materialID = currmaterial;
        faceList.add(face);
      }
      else if (strequal(tok, "usemtl"))   // usemtl
        currmaterial = this->findmaterial(tokenize(NULL, whiteSpace, saved));
      else if (strequal(tok, "mtllib")) { //  mtllib
        const char *mtlFileName = tokenize(NULL, whiteSpace, saved);
        if (this->loadmtl(mtlFileName, fileName) == 0) {
          console::out("objloader: Error loading %s", mtlFileName);
          return false;
        }
        continue;
      }
      else if (strequal(tok, "p")) {} // point
      else if (strequal(tok, "o")) {} // object name
      else if (strequal(tok, "s")) {} // smoothing
      else if (strequal(tok, "g")) {} // group
      else console::out("objloader: unknown command: %s in obj file %s at line %i, \"%s\"",
                        tok, fileName, lineNumber, currLine);
    }

    fclose(objFile);
    return true;
  }

  static inline bool cmp(obj::triangle t0, obj::triangle t1) {return t0.m < t1.m;}

  obj::obj(void) { memset(this, 0, sizeof(obj)); }

  // sort vertices faces
  struct vertexkey {
    INLINE vertexkey(void) {}
    INLINE vertexkey(int p_, int n_, int t_) : p(p_), n(n_), t(t_) {}
    bool operator == (const vertexkey &other) const {
      return (p == other.p) && (n == other.n) && (t == other.t);
    }
    bool operator < (const vertexkey &other) const {
      if (p != other.p) return p < other.p;
      if (n != other.n) return n < other.n;
      if (t != other.t) return t < other.t;
      return false;
    }
    int p,n,t;
  };

  // store face connectivity
  struct Poly { int v[4]; int mat; int n; };

  bool obj::load(const char *filename)
  {
    objloader loader;
    map<vertexkey, int> map;
    vector<Poly> polys;
    if (loader.loadobj(filename) == 0) return false;

    int vert_n = 0;
    loopv(loader.faceList) {
      const objloader::face *face = loader.faceList[i];
      if (face == NULL) {
        console::out("objloader: null face for %s", filename);
        return false;
      }
      if (face->vertexNum > 4) {
        console::out("objloader: too many vertices for %s", filename);
        return false;
      }
      int v[] = {0,0,0,0};
      for (int j = 0; j < face->vertexNum; ++j) {
        const int p = face->vertexID[j];
        const int n = face->normalID[j];
        const int t = face->textureID[j];
        const vertexkey key(p,n,t);
        const auto it = map.find(key);
        if (it == map.end())
          v[j] = map[key] = vert_n++;
        else
          v[j] = it->second;
      }
      const Poly p = {{v[0],v[1],v[2],v[3]},face->materialID,face->vertexNum};
      polys.add(p);
    }

    // No face defined
    if (polys.size() == 0) return true;

    // create triangles now
    vector<triangle> tris;
    for (auto poly = polys.begin(); poly != polys.end(); ++poly) {
      if (poly->n == 3) {
        const triangle tri(vec3i(poly->v[0], poly->v[1], poly->v[2]), poly->mat);
        tris.add(tri);
      } else {
        const triangle tri0(vec3i(poly->v[0], poly->v[1], poly->v[2]), poly->mat);
        const triangle tri1(vec3i(poly->v[0], poly->v[2], poly->v[3]), poly->mat);
        tris.add(tri0);
        tris.add(tri1);
      }
    }

    // sort triangle by material and create the material groups
    tris.sort(cmp);
    vector<matgroup> matGrp;
    int curr = tris[0].m;
    matGrp.add(matgroup(0,0,curr));
    loopv(tris)
      if (tris[i].m != curr) {
        curr = tris[i].m;
        matGrp.last().last = (int) (i-1);
        matGrp.add(matgroup((int)i,0,curr));
      }
    matGrp.last().last = tris.size() - 1;

    // we replace the undefined material by the default one if needed
    if (tris[0].m == -1) {
      auto mat = loader.allocmat();
      mat->setDefault();
      loader.materialList.add(mat);

      // first path the triangle
      const size_t matID = loader.materialList.size() - 1;
      loopv(tris)
        if (tris[i].m != -1)
          break;
        else
          tris[i].m = matID;

      // Then, their group
      ASSERT(matGrp[0].m == -1);
      matGrp[0].m = matID;
    }

    // Create all the vertices and store them
    const auto vertNum = map.size();
    vector<vertex> verts(vertNum);
    bool allPositionSet = true, allNormalSet = true, allTexCoordSet = true;
    for (auto it = map.begin(); it != map.end(); ++it) {
      const auto src = it->first;
      const auto dst = it->second;
      vertex &v = verts[dst];

      // Get the position if specified
      if (src.p != -1) {
        const objloader::vec *p = loader.vertexList[src.p];
        loopi(3) v.p[i] = float(p->e[i]);
      } else {
        v.p[0] = v.p[1] = v.p[2] = 0.f;
        allPositionSet = false;
      }

      // Get the normal if specified
      if (src.n != -1) {
        const objloader::vec *n = loader.normalList[src.n];
        loopi(3) v.p[i] = float(n->e[i]);
      } else {
        v.n[0] = v.n[1] = v.n[2] = 1.f;
        allNormalSet = false;
      }

      // Get the texture coordinates if specified
      if (src.t != -1) {
        const objloader::vec *t = loader.textureList[src.t];
        v.t[0] = float(t->e[0]);
        v.t[1] = float(t->e[1]);
      } else {
        v.t[0] = v.t[1] = 0.f;
        allTexCoordSet = false;
      }
    }

    if (!allPositionSet)
      console::out("objloader: some positions are unspecified for %s", filename);
    if (!allNormalSet)
      console::out("objloader: some normals are unspecified for %s", filename);
    if (!allTexCoordSet)
      console::out("objloader: some texture coordinates are unspecified for %s", filename);

    // Allocate the objmaterial
    auto mat = NEWAE(material, loader.materialList.size());
    memset(mat, 0, sizeof(material) * loader.materialList.size());

#define COPY_FIELD(NAME)\
    if (from.NAME) {\
      const size_t len = strlen(from.NAME);\
      to.NAME = NEWAE(char, len + 1);\
      if (len) memcpy(to.NAME, from.NAME, len);\
      to.NAME[len] = 0;\
    }
    loopv(loader.materialList) {
      const objloader::mat &from = *loader.materialList[i];
      ASSERT(loader.materialList[i] != NULL);
      material &to = mat[i];
      COPY_FIELD(name);
      COPY_FIELD(map_Ka);
      COPY_FIELD(map_Kd);
      COPY_FIELD(map_D);
      COPY_FIELD(map_Bump);
    }
#undef COPY_FIELD

    // Now return the properly allocated obj
    memset(this, 0, sizeof(obj));
    this->triNum = tris.size();
    this->vertNum = verts.size();
    this->grpNum = matGrp.size();
    this->matNum = loader.materialList.size();
    if (this->triNum) {
      this->tri = NEWAE(triangle, this->triNum);
      memcpy(this->tri, &tris[0],  sizeof(triangle) * this->triNum);
    }
    if (this->vertNum) {
      this->vert = NEWAE(vertex, this->vertNum);
      memcpy(this->vert, &verts[0], sizeof(vertex) * this->vertNum);
    }
    if (this->grpNum) {
      this->grp = NEWAE(matgroup, this->grpNum);
      memcpy(this->grp,  &matGrp[0],  sizeof(matgroup) * this->grpNum);
    }
    this->mat = mat;
    console::out("objloader: %s, %i triangles", filename, triNum);
    console::out("objloader: %s, %i vertices", filename, vertNum);
    console::out("objloader: %s, %i groups", filename, grpNum);
    console::out("objloader: %s, %i materials", filename, matNum);
    return true;
  }

  obj::~obj(void) {
    SAFE_DELETEA(tri);
    SAFE_DELETEA(vert);
    SAFE_DELETEA(grp);
    loopi(int(matNum)) {
      SAFE_DELETEA(mat[i].name);
      SAFE_DELETEA(mat[i].map_Ka);
      SAFE_DELETEA(mat[i].map_Kd);
      SAFE_DELETEA(mat[i].map_D);
      SAFE_DELETEA(mat[i].map_Bump);
    }
    SAFE_DELETEA(mat);
  }
} // namespace cube

