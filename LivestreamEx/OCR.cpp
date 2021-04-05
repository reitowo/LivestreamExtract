#include "OCR.h"

#include <tesseract/baseapi.h>
#include "../common.h"

static tesseract::TessBaseAPI* ocr;

void ocrInit()
{
	ocr = new tesseract::TessBaseAPI();
	ocr->Init(tesseractDataPath.c_str(), "eng");
}

char* ocrSmm2Rating(cv::Mat mat)
{
	ocr->SetImage(mat.data, mat.size().width, mat.size().height, mat.channels(),
	              mat.step1());
	ocr->SetSourceResolution(70);
	return ocr->GetUTF8Text();
}
