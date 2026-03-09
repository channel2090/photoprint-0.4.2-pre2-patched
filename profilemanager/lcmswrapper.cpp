/*
 * lcmswrapper.cpp - encapsulates typical "user" functions of LittleCMS,
 * providing a Profile and Transform class
 *
 * Copyright (c) 2004-2008 by Alastair M. Robinson
 * Distributed under the terms of the GNU General Public License -
 * see the file named "COPYING" for more details.
 *
 * 2005-05-01: No longer depends on ini.h - the neccesary data for generating
 *             rough-cast profiles is now encapsulated in suitable classes.
 * 2006-09-04: Added filename, since LCMS doesn't provide pointer to raw data
 *             which is needed for TIFF embedding...
 *
 * TODO: pixel type, support Lab, XYZ, etc.
 *
 */

#include <iostream>
#include <fstream>
#include <cstring>
#include <string.h>
#include <strings.h>
#include <cstdlib>

#include "../support/debug.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gettext.h"
#define _(x) gettext(x)
#define N_(x) gettext_noop(x)

#include "imagesource.h"
#include "lcmswrapper.h"

using namespace std;

static const int ICC_HEADER_SIZE = 128;

static const char *GetProfileInfoText(cmsHPROFILE prof, cmsInfoType info)
{
	static char buffer[1024];

	buffer[0] = '\0';

	cmsUInt32Number needed =
		cmsGetProfileInfoASCII(prof, info, "en", "US", NULL, 0);

	if(needed == 0)
		return "unknown";

	if(needed > sizeof(buffer))
		needed = sizeof(buffer);

	cmsGetProfileInfoASCII(prof, info, "en", "US", buffer, needed);
	buffer[sizeof(buffer) - 1] = '\0';

	return buffer[0] ? buffer : "unknown";
}

CMSRGBPrimaries CMSPrimaries_Rec709(.64,.33,.3,.6,.15,.06);
CMSRGBPrimaries CMSPrimaries_Adobe(0.64, 0.33,0.21, 0.71,0.15, 0.06);
CMSRGBPrimaries CMSPrimaries_NTSC(0.67, 0.33, 0.21, 0.71,0.14, 0.08);
CMSRGBPrimaries CMSPrimaries_EBU(0.64, 0.33, 0.29, 0.60, 0.15, 0.06);
CMSRGBPrimaries CMSPrimaries_SMPTE(0.63, 0.34, 0.31, 0.595, 0.155, 0.070);
CMSRGBPrimaries CMSPrimaries_HDTV(0.670, 0.330, 0.210, 0.710,0.150, 0.060);
CMSRGBPrimaries CMSPrimaries_CIE(0.7355,0.2645,0.2658,0.7243,0.1669,0.0085);

CMSProfile::CMSProfile(const char *fn)
	: md5(NULL), prof(NULL), generated(false), filename(NULL), buffer(NULL), buflen(0)
{
	if(!fn)
		throw "NULL profile filename provided";

	cmsSetLogErrorHandler(NULL);

	filename = strdup(fn);

	if(!(prof = cmsOpenProfileFromFile(filename, "r")))
		throw "Can't open profile";

	CalcMD5();
}

void CMSProfile::CalcMD5()
{
	if(md5)
	{
		delete md5;
		md5 = NULL;
	}

	if(generated)
	{
		Debug[TRACE] << "Saving profile to RAM for MD5 calculation." << endl;

		cmsUInt32Number plen = 0;
		cmsSaveProfileToMem(prof, NULL, &plen);

		if(plen > 0)
		{
			Debug[TRACE] << "Plen = " << plen << endl;

			if(buffer)
			{
				free(buffer);
				buffer = NULL;
			}

			buflen = (int) plen;
			buffer = (char *) malloc(buflen);

			if(buffer && cmsSaveProfileToMem(prof, buffer, &plen))
			{
				Debug[TRACE] << "Saved successfully" << endl;

				if(buflen > ICC_HEADER_SIZE)
					md5 = new MD5Digest(buffer + ICC_HEADER_SIZE, buflen - ICC_HEADER_SIZE);
				else
					md5 = new MD5Digest(buffer, buflen);
			}
		}
	}
	else if(filename)
	{
		ifstream f(filename, ios::in | ios::binary);
		f.seekg(0, ios::end);
		int filelen = (int) f.tellg();
		f.seekg(0);

		char *data = (char *) malloc(filelen);
		f.read((char *) data, filelen);
		f.close();

		if(filelen > ICC_HEADER_SIZE)
			md5 = new MD5Digest(data + ICC_HEADER_SIZE, filelen - ICC_HEADER_SIZE);
		else
			md5 = new MD5Digest(data, filelen);

		free(data);
	}
	else if(buffer && buflen > 0)
	{
		if(buflen > ICC_HEADER_SIZE)
			md5 = new MD5Digest(buffer + ICC_HEADER_SIZE, buflen - ICC_HEADER_SIZE);
		else
			md5 = new MD5Digest(buffer, buflen);
	}
}

CMSProfile::CMSProfile(CMSRGBPrimaries &primaries,CMSRGBGamma &gamma,CMSWhitePoint &whitepoint)
	: md5(NULL), prof(NULL), generated(true), filename(NULL), buffer(NULL), buflen(0)
{
	if(!(prof = cmsCreateRGBProfile(&whitepoint.whitepoint, &primaries, gamma.gammatables)))
		throw "Can't create virtual RGB profile";

	CalcMD5();
}

CMSProfile::CMSProfile(CMSGamma &gamma,CMSWhitePoint &whitepoint)
	: md5(NULL), prof(NULL), generated(true), filename(NULL), buffer(NULL), buflen(0)
{
	if(!(prof = cmsCreateGrayProfile(&whitepoint.whitepoint, gamma.GetGammaTable())))
		throw "Can't create virtual Grey profile";

	CalcMD5();
}

CMSProfile::CMSProfile(CMSWhitePoint &whitepoint)
	: md5(NULL), prof(NULL), generated(true), filename(NULL), buffer(NULL), buflen(0)
{
	if(!(prof = cmsCreateLab4Profile(&whitepoint.whitepoint)))
		throw "Can't create virtual LAB profile";

	CalcMD5();
}

CMSProfile::CMSProfile(char *srcbuf,int length)
	: md5(NULL), prof(NULL), generated(false), filename(NULL), buffer(NULL), buflen(0)
{
	buffer = (char *) malloc(length);
	buflen = length;

	memcpy(buffer, srcbuf, buflen);

	if(!(prof = cmsOpenProfileFromMem(buffer, buflen)))
		throw "Can't open profile";

	CalcMD5();
}

CMSProfile::CMSProfile()
	: md5(NULL), prof(NULL), generated(true), filename(NULL), buffer(NULL), buflen(0)
{
	if(!(prof = cmsCreate_sRGBProfile()))
		throw "Can't create virtual sRGB profile";

	CalcMD5();
}

CMSProfile::CMSProfile(const CMSProfile &src)
	: md5(NULL), prof(NULL), generated(src.generated), filename(NULL), buffer(NULL), buflen(0)
{
	Debug[TRACE] << "In CMSProfile Copy Constructor" << endl;

	if(src.filename)
	{
		filename = strdup(src.filename);

		if(!(prof = cmsOpenProfileFromFile(filename, "r")))
			throw "Can't open profile";
	}
	else
	{
		buffer = (char *) malloc(src.buflen);
		buflen = src.buflen;
		memcpy(buffer, src.buffer, src.buflen);

		if(!(prof = cmsOpenProfileFromMem(buffer, buflen)))
			throw "Can't open profile";
	}

	if(src.md5)
		md5 = new MD5Digest(*src.md5);
}

CMSProfile::~CMSProfile()
{
	if(filename)
		free(filename);

	if(prof)
		cmsCloseProfile(prof);

	if(buffer)
		free(buffer);

	if(md5)
		delete md5;
}

bool CMSProfile::operator==(const CMSProfile &other)
{
	if(md5 && other.md5)
		return (*md5 == *other.md5);

	return false;
}

bool CMSProfile::IsDeviceLink()
{
	return (cmsGetDeviceClass(prof) == cmsSigLinkClass);
}

bool CMSProfile::IsV4()
{
	Debug[TRACE] << "Profile version: " << cmsGetProfileVersion(prof) << endl;
	return (cmsGetProfileVersion(prof) >= 4.0);
}

enum IS_TYPE CMSProfile::GetColourSpace()
{
	cmsColorSpaceSignature sig = cmsGetColorSpace(prof);

	switch(sig)
	{
		case cmsSigGrayData:
			return(IS_TYPE_GREY);
		case cmsSigRgbData:
			return(IS_TYPE_RGB);
		case cmsSigCmykData:
			return(IS_TYPE_CMYK);
		case cmsSigLabData:
			return(IS_TYPE_LAB);
		default:
			return(IS_TYPE_NULL);
	}
}

enum IS_TYPE CMSProfile::GetDeviceLinkOutputSpace()
{
	if(!IsDeviceLink())
		throw "GetDeviceLinkOutputSpace() can only be used on DeviceLink profiles!";

	cmsColorSpaceSignature sig = cmsGetPCS(prof);

	switch(sig)
	{
		case cmsSigGrayData:
			return(IS_TYPE_GREY);
		case cmsSigRgbData:
			return(IS_TYPE_RGB);
		case cmsSigCmykData:
			return(IS_TYPE_CMYK);
		case cmsSigLabData:
			return(IS_TYPE_LAB);
		default:
			return(IS_TYPE_NULL);
	}
}

const char *CMSProfile::GetName()
{
	return GetProfileInfoText(prof, cmsInfoDescription);
}

const char *CMSProfile::GetManufacturer()
{
	return GetProfileInfoText(prof, cmsInfoManufacturer);
}

const char *CMSProfile::GetModel()
{
	return GetProfileInfoText(prof, cmsInfoModel);
}

const char *CMSProfile::GetDescription()
{
	return GetProfileInfoText(prof, cmsInfoDescription);
}

const char *CMSProfile::GetInfo()
{
	return GetProfileInfoText(prof, cmsInfoDescription);
}

const char *CMSProfile::GetCopyright()
{
	return GetProfileInfoText(prof, cmsInfoCopyright);
}

MD5Digest *CMSProfile::GetMD5()
{
	return(md5);
}

const char *CMSProfile::GetFilename()
{
	return(filename);
}

bool CMSProfile::Save(const char *outfn)
{
	try
	{
		if(filename)
		{
			ifstream f(filename, ios::in | ios::binary);
			f.exceptions(fstream::badbit);

			f.seekg(0, ios::end);
			buflen = (int) f.tellg();
			f.seekg(0);

			buffer = (char *) malloc(buflen);
			f.read((char *) buffer, buflen);
			f.close();
		}

		if(outfn)
		{
			Debug[TRACE] << "Saving buffer: " << long(buffer) << ", length: " << buflen << endl;

			ofstream f(outfn, ios::out | ios::binary);
			f.write(buffer, buflen);
			f.close();
		}

		if(filename)
		{
			free(buffer);
			buffer = NULL;
			buflen = 0;
		}
	}
	catch(fstream::failure &)
	{
		return(false);
	}

	return(true);
}

std::ostream& operator<<(std::ostream &s,CMSProfile &cp)
{
	return(s << cp.GetDescription());
}

CMSTransform::CMSTransform()
	: inputtype(IS_TYPE_NULL), outputtype(IS_TYPE_NULL), transform(NULL)
{
}

CMSTransform::CMSTransform(CMSProfile *in,CMSProfile *out,LCMSWrapper_Intent intent)
	: transform(NULL)
{
	inputtype = in->GetColourSpace();
	outputtype = out->GetColourSpace();
	MakeTransform(in, out, intent);
}

CMSTransform::CMSTransform(CMSProfile *devicelink,LCMSWrapper_Intent intent)
	: transform(NULL)
{
	inputtype = devicelink->GetColourSpace();
	outputtype = devicelink->GetDeviceLinkOutputSpace();
	MakeTransform(devicelink, NULL, intent);
}

CMSTransform::CMSTransform(CMSProfile *profiles[],int profilecount,LCMSWrapper_Intent intent)
	: transform(NULL)
{
	inputtype = profiles[0]->GetColourSpace();

	if(profiles[profilecount - 1]->IsDeviceLink())
		outputtype = profiles[profilecount - 1]->GetDeviceLinkOutputSpace();
	else
		outputtype = profiles[profilecount - 1]->GetColourSpace();

	cmsHPROFILE *p;

	if((p = (cmsHPROFILE *) malloc(sizeof(cmsHPROFILE) * profilecount)))
	{
		for(int i = 0; i < profilecount; ++i)
			p[i] = profiles[i]->prof;

		int it,ot;

		switch(inputtype)
		{
			case IS_TYPE_GREY: it = TYPE_GRAY_16_REV; break;
			case IS_TYPE_RGB:  it = TYPE_RGB_16; break;
			case IS_TYPE_CMYK: it = TYPE_CMYK_16; break;
			case IS_TYPE_LAB:  it = TYPE_Lab_16; break;
			default: throw "Unsupported colour space (input)";
		}

		switch(outputtype)
		{
			case IS_TYPE_GREY: it = it; ot = TYPE_GRAY_16_REV; break;
			case IS_TYPE_RGB:  ot = TYPE_RGB_16; break;
			case IS_TYPE_CMYK: ot = TYPE_CMYK_16; break;
			case IS_TYPE_LAB:  ot = TYPE_Lab_16; break;
			default: throw "Unsupported colour space (output)";
		}

		transform = cmsCreateMultiprofileTransform(
			p,
			profilecount,
			it,
			ot,
			CMS_GetLCMSIntent(intent),
			CMS_GetLCMSFlags(intent)
		);

		free(p);
	}
	else
	{
		throw "Can't create multi-profile transform";
	}
}

CMSTransform::~CMSTransform()
{
	if(transform)
		cmsDeleteTransform(transform);
}

void CMSTransform::Transform(unsigned short *src,unsigned short *dst,int pixels)
{
	cmsDoTransform(transform, src, dst, pixels);
}

IS_TYPE CMSTransform::GetInputColourSpace()
{
	return(inputtype);
}

IS_TYPE CMSTransform::GetOutputColourSpace()
{
	return(outputtype);
}

void CMSTransform::MakeTransform(CMSProfile *in,CMSProfile *out,LCMSWrapper_Intent intent)
{
	int it,ot;

	switch(GetInputColourSpace())
	{
		case IS_TYPE_GREY: it = TYPE_GRAY_16_REV; break;
		case IS_TYPE_RGB:  it = TYPE_RGB_16; break;
		case IS_TYPE_CMYK: it = TYPE_CMYK_16; break;
		case IS_TYPE_LAB:  it = TYPE_Lab_16; break;
		default: throw "Unsupported colour space (input)";
	}

	switch(GetOutputColourSpace())
	{
		case IS_TYPE_GREY: ot = TYPE_GRAY_16_REV; break;
		case IS_TYPE_RGB:  ot = TYPE_RGB_16; break;
		case IS_TYPE_CMYK: ot = TYPE_CMYK_16; break;
		case IS_TYPE_LAB:  ot = TYPE_Lab_16; break;
		default: throw "Unsupported colour space (output)";
	}

	transform = cmsCreateTransform(
		in->prof,
		it,
		(out ? out->prof : NULL),
		ot,
		CMS_GetLCMSIntent(intent),
		CMS_GetLCMSFlags(intent)
	);
}

// FIXME - this will need to be updated if LCMS supports new intents at some point
// in the future...

static const char *intent_names[] =
{
	N_("Perceptual"),
	N_("Relative Colorimetric"),
	N_("Relative Colorimetric with BPC"),
	N_("Saturation"),
	N_("Absolute Colorimetric")
};

static const char *intent_descriptions[] =
{
	N_("Aims for pleasing photographic results by preserving relationships between colours, and avoiding clipping of unattainable colours"),
	N_("Attempts to preserve the original colours, relative to white-point differences."),
	N_("Attempts to preserve the original colours, relative to white-point differences.  Uses Black Point Compensation to avoid clipping of highlights and shadows."),
	N_("Attempts to provide brighter, more saturated colours, at the expense of colour relationships."),
	N_("Mimics the original colours (including white point) as closely as possible, for side-by-side comparisons.  Clips unattainable colours.")
};

static const int intent_lcmsintents[] =
{
	INTENT_PERCEPTUAL,
	INTENT_RELATIVE_COLORIMETRIC,
	INTENT_RELATIVE_COLORIMETRIC,
	INTENT_SATURATION,
	INTENT_ABSOLUTE_COLORIMETRIC
};

static const int intent_lcmsflags[] =
{
	0,
	0,
	cmsFLAGS_BLACKPOINTCOMPENSATION,
	0,
	0
};

int CMS_GetIntentCount()
{
	return(sizeof(intent_names) / sizeof(intent_names[0]));
}

const char *CMS_GetIntentName(int intent)
{
	if(intent < CMS_GetIntentCount() && intent >= 0)
		return(gettext(intent_names[intent]));

	return(NULL);
}

const char *CMS_GetIntentDescription(int intent)
{
	if(intent < CMS_GetIntentCount() && intent >= 0)
		return(gettext(intent_descriptions[intent]));

	return(NULL);
}

const int CMS_GetLCMSIntent(int intent)
{
	if(intent < CMS_GetIntentCount() && intent >= 0)
		return(intent_lcmsintents[intent]);

	return(0);
}

const int CMS_GetLCMSFlags(int intent)
{
	if(intent < CMS_GetIntentCount() && intent >= 0)
		return(intent_lcmsflags[intent]);

	return(0);
}

CMSProofingTransform::CMSProofingTransform(CMSProfile *in,CMSProfile *out,CMSProfile *proof,int proofintent,int viewintent)
	: CMSTransform()
{
	inputtype = in->GetColourSpace();

	if(out)
		outputtype = out->GetColourSpace();
	else
		outputtype = in->GetDeviceLinkOutputSpace();

	int it,ot;

	switch(inputtype)
	{
		case IS_TYPE_GREY: it = TYPE_GRAY_16_REV; break;
		case IS_TYPE_RGB:  it = TYPE_RGB_16; break;
		case IS_TYPE_CMYK: it = TYPE_CMYK_16; break;
		case IS_TYPE_LAB:  it = TYPE_Lab_16; break;
		default: throw "Unsupported colour space (input)";
	}

	switch(outputtype)
	{
		case IS_TYPE_GREY: ot = TYPE_GRAY_16_REV; break;
		case IS_TYPE_RGB:  ot = TYPE_RGB_16; break;
		case IS_TYPE_CMYK: ot = TYPE_CMYK_16; break;
		case IS_TYPE_LAB:  ot = TYPE_Lab_16; break;
		default: throw "Unsupported colour space (output)";
	}

	transform = cmsCreateProofingTransform(
		in->prof,
		it,
		(out ? out->prof : NULL),
		ot,
		proof->prof,
		CMS_GetLCMSIntent(proofintent),
		CMS_GetLCMSIntent(viewintent),
		CMS_GetLCMSFlags(proofintent) | cmsFLAGS_SOFTPROOFING
	);
}

CMSProofingTransform::CMSProofingTransform(CMSProfile *devicelink,CMSProfile *proof,int proofintent,int viewintent)
	: CMSTransform()
{
	inputtype = devicelink->GetColourSpace();
	outputtype = devicelink->GetDeviceLinkOutputSpace();

	int it,ot;

	switch(inputtype)
	{
		case IS_TYPE_GREY: it = TYPE_GRAY_16_REV; break;
		case IS_TYPE_RGB:  it = TYPE_RGB_16; break;
		case IS_TYPE_CMYK: it = TYPE_CMYK_16; break;
		case IS_TYPE_LAB:  it = TYPE_Lab_16; break;
		default: throw "Unsupported colour space (input)";
	}

	switch(outputtype)
	{
		case IS_TYPE_GREY: ot = TYPE_GRAY_16_REV; break;
		case IS_TYPE_RGB:  ot = TYPE_RGB_16; break;
		case IS_TYPE_CMYK: ot = TYPE_CMYK_16; break;
		case IS_TYPE_LAB:  ot = TYPE_Lab_16; break;
		default: throw "Unsupported colour space (output)";
	}

	transform = cmsCreateProofingTransform(
		devicelink->prof,
		it,
		NULL,
		ot,
		proof->prof,
		CMS_GetLCMSIntent(proofintent),
		CMS_GetLCMSIntent(viewintent),
		CMS_GetLCMSFlags(proofintent) | cmsFLAGS_SOFTPROOFING
	);
}