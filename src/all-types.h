#ifndef ALL_TYPES_H
#define ALL_TYPES_H

#include "util.h"

class Document;
class EmbeddedPDF;
class SPage;
class DrawingLayer;
class NormalLayer;
class PDFLayer;
class PenStroke;
class EraserStroke;
class TemporaryLayer;
class FadingStroke;
class PagePicture;
class DrawingLayerPicture;
class PDFLayerPicture;
class PageWidget;
class MainWindow;
class TabletSettings;
struct _PopplerDocument;
struct _PopplerPage;

struct Point {
	int x, y;
	Point(int _x, int _y) :
	    x(_x), y(_y) {
	}
};

typedef std::variant<PenStroke*, EraserStroke*> ptr_Stroke;
typedef variant_unique<PenStroke, EraserStroke> unique_ptr_Stroke;

struct stroke_unique_to_ptr_helper {
	typedef unique_ptr_Stroke in_type;
	typedef ptr_Stroke out_type;
	out_type operator()(const in_type& p) {
		return get(p);
	}
};

typedef std::variant<NormalLayer*, PDFLayer*> ptr_Layer;
typedef variant_unique<NormalLayer, PDFLayer> unique_ptr_Layer;

struct layer_unique_to_ptr_helper {
	typedef unique_ptr_Layer in_type;
	typedef ptr_Layer out_type;
	out_type operator()(const in_type& p) {
		return get(p);
	}
};

typedef std::variant<DrawingLayerPicture*, PDFLayerPicture*> ptr_LayerPicture;
typedef variant_unique<DrawingLayerPicture, PDFLayerPicture> unique_ptr_LayerPicture;

struct layer_picture_unique_to_ptr_helper {
	typedef unique_ptr_LayerPicture in_type;
	typedef ptr_LayerPicture out_type;
	out_type operator()(const in_type& p) {
		return get(p);
	}
};

#endif
