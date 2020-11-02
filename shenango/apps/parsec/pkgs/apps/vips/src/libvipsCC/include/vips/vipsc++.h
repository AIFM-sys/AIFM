
// headers for package arithmetic
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
VImage abs() throw( VError );
VImage acos() throw( VError );
VImage add( VImage ) throw( VError );
VImage asin() throw( VError );
VImage atan() throw( VError );
double avg() throw( VError );
double point_bilinear( double, double, int ) throw( VError );
VImage bandmean() throw( VError );
VImage ceil() throw( VError );
VImage cos() throw( VError );
VImage cross_phase( VImage ) throw( VError );
double deviate() throw( VError );
VImage divide( VImage ) throw( VError );
VImage exp10() throw( VError );
VImage expn( double ) throw( VError );
VImage expn( std::vector<double> ) throw( VError );
VImage exp() throw( VError );
VImage floor() throw( VError );
VImage invert() throw( VError );
VImage lin( double, double ) throw( VError );
static VImage linreg( std::vector<VImage>, std::vector<double> ) throw( VError );
VImage lin( std::vector<double>, std::vector<double> ) throw( VError );
VImage log10() throw( VError );
VImage log() throw( VError );
double max() throw( VError );
std::complex<double> maxpos() throw( VError );
double maxpos_avg( double&, double& ) throw( VError );
VDMask measure( int, int, int, int, int, int ) throw( VError );
double min() throw( VError );
std::complex<double> minpos() throw( VError );
VImage multiply( VImage ) throw( VError );
VImage pow( double ) throw( VError );
VImage pow( std::vector<double> ) throw( VError );
VImage recomb( VDMask ) throw( VError );
VImage remainder( VImage ) throw( VError );
VImage remainder( double ) throw( VError );
VImage remainder( std::vector<double> ) throw( VError );
VImage rint() throw( VError );
VImage sign() throw( VError );
VImage sin() throw( VError );
VDMask stats() throw( VError );
VImage subtract( VImage ) throw( VError );
VImage tan() throw( VError );

// headers for package boolean
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
VImage andimage( VImage ) throw( VError );
VImage andimage( int ) throw( VError );
VImage andimage( std::vector<double> ) throw( VError );
VImage orimage( VImage ) throw( VError );
VImage orimage( int ) throw( VError );
VImage orimage( std::vector<double> ) throw( VError );
VImage eorimage( VImage ) throw( VError );
VImage eorimage( int ) throw( VError );
VImage eorimage( std::vector<double> ) throw( VError );
VImage shiftleft( std::vector<double> ) throw( VError );
VImage shiftleft( int ) throw( VError );
VImage shiftright( std::vector<double> ) throw( VError );
VImage shiftright( int ) throw( VError );

// headers for package cimg
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
VImage greyc( int, double, double, double, double, double, double, double, double, int, int ) throw( VError );
VImage greyc_mask( VImage, int, double, double, double, double, double, double, double, double, int, int ) throw( VError );

// headers for package colour
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
VImage LCh2Lab() throw( VError );
VImage LCh2UCS() throw( VError );
VImage Lab2LCh() throw( VError );
VImage Lab2LabQ() throw( VError );
VImage Lab2LabS() throw( VError );
VImage Lab2UCS() throw( VError );
VImage Lab2XYZ() throw( VError );
VImage Lab2XYZ_temp( double, double, double ) throw( VError );
VImage Lab2disp( VDisplay ) throw( VError );
VImage LabQ2LabS() throw( VError );
VImage LabQ2Lab() throw( VError );
VImage LabQ2XYZ() throw( VError );
VImage LabQ2disp( VDisplay ) throw( VError );
VImage LabS2LabQ() throw( VError );
VImage LabS2Lab() throw( VError );
VImage UCS2LCh() throw( VError );
VImage UCS2Lab() throw( VError );
VImage UCS2XYZ() throw( VError );
VImage XYZ2Lab() throw( VError );
VImage XYZ2Lab_temp( double, double, double ) throw( VError );
VImage XYZ2UCS() throw( VError );
VImage XYZ2Yxy() throw( VError );
VImage XYZ2disp( VDisplay ) throw( VError );
VImage XYZ2sRGB() throw( VError );
VImage Yxy2XYZ() throw( VError );
VImage dE00_fromLab( VImage ) throw( VError );
VImage dECMC_fromLab( VImage ) throw( VError );
VImage dECMC_fromdisp( VImage, VDisplay ) throw( VError );
VImage dE_fromLab( VImage ) throw( VError );
VImage dE_fromXYZ( VImage ) throw( VError );
VImage dE_fromdisp( VImage, VDisplay ) throw( VError );
VImage disp2Lab( VDisplay ) throw( VError );
VImage disp2XYZ( VDisplay ) throw( VError );
VImage float2rad() throw( VError );
VImage icc_ac2rc( char* ) throw( VError );
VImage icc_export_depth( int, char*, int ) throw( VError );
VImage icc_import( char*, int ) throw( VError );
VImage icc_import_embedded( int ) throw( VError );
VImage icc_transform( char*, char*, int ) throw( VError );
VImage lab_morph( VDMask, double, double, double, double ) throw( VError );
VImage rad2float() throw( VError );
VImage sRGB2XYZ() throw( VError );

// headers for package conversion
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
static VImage gaussnoise( int, int, double, double ) throw( VError );
VImage bandjoin( VImage ) throw( VError );
static VImage black( int, int, int ) throw( VError );
VImage c2amph() throw( VError );
VImage c2imag() throw( VError );
VImage c2real() throw( VError );
VImage c2rect() throw( VError );
VImage clip2fmt( int ) throw( VError );
VImage copy() throw( VError );
VImage copy_file() throw( VError );
VImage copy_morph( int, int, int ) throw( VError );
VImage copy_swap() throw( VError );
VImage copy_set( int, double, double, int, int ) throw( VError );
VImage extract_area( int, int, int, int ) throw( VError );
VImage extract_areabands( int, int, int, int, int, int ) throw( VError );
VImage extract_band( int ) throw( VError );
VImage extract_bands( int, int ) throw( VError );
VImage extract( int, int, int, int, int ) throw( VError );
VImage falsecolour() throw( VError );
VImage fliphor() throw( VError );
VImage flipver() throw( VError );
static VImage gbandjoin( std::vector<VImage> ) throw( VError );
VImage grid( int, int, int ) throw( VError );
VImage insert( VImage, int, int ) throw( VError );
VImage insert( VImage, std::vector<int>, std::vector<int> ) throw( VError );
VImage insert_noexpand( VImage, int, int ) throw( VError );
VImage embed( int, int, int, int, int ) throw( VError );
VImage lrjoin( VImage ) throw( VError );
static VImage mask2vips( VDMask ) throw( VError );
VImage msb() throw( VError );
VImage msb_band( int ) throw( VError );
VImage replicate( int, int ) throw( VError );
VImage ri2c( VImage ) throw( VError );
VImage rot180() throw( VError );
VImage rot270() throw( VError );
VImage rot90() throw( VError );
VImage scale() throw( VError );
VImage scaleps() throw( VError );
VImage subsample( int, int ) throw( VError );
char* system( char* ) throw( VError );
VImage system_image( char*, char*, char*, char*& ) throw( VError );
VImage tbjoin( VImage ) throw( VError );
static VImage text( char*, char*, int, int, int ) throw( VError );
VDMask vips2mask() throw( VError );
VImage wrap( int, int ) throw( VError );
VImage zoom( int, int ) throw( VError );

// headers for package convolution
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
VImage addgnoise( double ) throw( VError );
VImage compass( VIMask ) throw( VError );
VImage contrast_surface( int, int ) throw( VError );
VImage conv( VIMask ) throw( VError );
VImage conv( VDMask ) throw( VError );
VImage convsep( VIMask ) throw( VError );
VImage convsep( VDMask ) throw( VError );
VImage fastcor( VImage ) throw( VError );
VImage gradcor( VImage ) throw( VError );
VImage gradient( VIMask ) throw( VError );
VImage grad_x() throw( VError );
VImage grad_y() throw( VError );
VImage lindetect( VIMask ) throw( VError );
VImage sharpen( int, double, double, double, double, double ) throw( VError );
VImage spcor( VImage ) throw( VError );

// headers for package deprecated
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
VImage clip() throw( VError );
VImage c2ps() throw( VError );
VImage resize_linear( int, int ) throw( VError );
VImage cmulnorm( VImage ) throw( VError );
VImage fav4( VImage, VImage, VImage ) throw( VError );
VImage gadd( double, double, VImage, double ) throw( VError );
VImage icc_export( char*, int ) throw( VError );
VImage litecor( VImage, int, double ) throw( VError );
VImage affine( double, double, double, double, double, double, int, int, int, int ) throw( VError );
VImage clip2c() throw( VError );
VImage clip2cm() throw( VError );
VImage clip2d() throw( VError );
VImage clip2dcm() throw( VError );
VImage clip2f() throw( VError );
VImage clip2i() throw( VError );
VImage convsub( VIMask, int, int ) throw( VError );
VImage convf( VDMask ) throw( VError );
VImage convsepf( VDMask ) throw( VError );
VImage clip2s() throw( VError );
VImage clip2ui() throw( VError );
VImage insertplace( VImage, std::vector<int>, std::vector<int> ) throw( VError );
VImage clip2us() throw( VError );
VImage slice( double, double ) throw( VError );
VImage segment( int& ) throw( VError );
void line( int, int, int, int, int ) throw( VError );
VImage thresh( double ) throw( VError );
VImage convf_raw( VDMask ) throw( VError );
VImage conv_raw( VIMask ) throw( VError );
VImage contrast_surface_raw( int, int ) throw( VError );
VImage convsepf_raw( VDMask ) throw( VError );
VImage convsep_raw( VIMask ) throw( VError );
VImage fastcor_raw( VImage ) throw( VError );
VImage gradcor_raw( VImage ) throw( VError );
VImage spcor_raw( VImage ) throw( VError );
VImage lhisteq_raw( int, int ) throw( VError );
VImage stdif_raw( double, double, double, double, int, int ) throw( VError );
VImage rank_raw( int, int, int ) throw( VError );
VImage dilate_raw( VIMask ) throw( VError );
VImage erode_raw( VIMask ) throw( VError );
VImage similarity_area( double, double, double, double, int, int, int, int ) throw( VError );
VImage similarity( double, double, double, double ) throw( VError );

// headers for package format
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
static VImage csv2vips( char* ) throw( VError );
static VImage jpeg2vips( char* ) throw( VError );
static VImage magick2vips( char* ) throw( VError );
static VImage png2vips( char* ) throw( VError );
static VImage exr2vips( char* ) throw( VError );
static VImage ppm2vips( char* ) throw( VError );
static VImage analyze2vips( char* ) throw( VError );
static VImage tiff2vips( char* ) throw( VError );
void vips2csv( char* ) throw( VError );
void vips2jpeg( char* ) throw( VError );
void vips2mimejpeg( int ) throw( VError );
void vips2png( char* ) throw( VError );
void vips2ppm( char* ) throw( VError );
void vips2tiff( char* ) throw( VError );

// headers for package freq_filt
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
static VImage create_fmask( int, int, int, double, double, double, double, double ) throw( VError );
VImage disp_ps() throw( VError );
VImage flt_image_freq( int, double, double, double, double, double ) throw( VError );
static VImage fractsurf( int, double ) throw( VError );
VImage freqflt( VImage ) throw( VError );
VImage fwfft() throw( VError );
VImage rotquad() throw( VError );
VImage invfft() throw( VError );
VImage phasecor_fft( VImage ) throw( VError );
VImage invfftr() throw( VError );

// headers for package histograms_lut
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
VImage gammacorrect( double ) throw( VError );
VImage heq( int ) throw( VError );
VImage hist( int ) throw( VError );
VImage histcum() throw( VError );
VImage histeq() throw( VError );
VImage hist_indexed( VImage ) throw( VError );
VImage histgr( int ) throw( VError );
VImage histnD( int ) throw( VError );
VImage histnorm() throw( VError );
VImage histplot() throw( VError );
VImage histspec( VImage ) throw( VError );
VImage hsp( VImage ) throw( VError );
static VImage identity( int ) throw( VError );
static VImage identity_ushort( int, int ) throw( VError );
int ismonotonic() throw( VError );
VImage lhisteq( int, int ) throw( VError );
int mpercent( double ) throw( VError );
static VImage invertlut( VDMask, int ) throw( VError );
static VImage buildlut( VDMask ) throw( VError );
VImage maplut( VImage ) throw( VError );
VImage project( VImage& ) throw( VError );
VImage stdif( double, double, double, double, int, int ) throw( VError );
VImage tone_analyse( double, double, double, double, double, double ) throw( VError );
static VImage tone_build( double, double, double, double, double, double, double, double ) throw( VError );
static VImage tone_build_range( int, int, double, double, double, double, double, double, double, double ) throw( VError );
VImage tone_map( VImage ) throw( VError );

// headers for package inplace
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
void circle( int, int, int, int ) throw( VError );
VImage flood_copy( int, int, std::vector<double> ) throw( VError );
VImage flood_blob_copy( int, int, std::vector<double> ) throw( VError );
VImage flood_other_copy( VImage, int, int, int ) throw( VError );
void insertplace( VImage, int, int ) throw( VError );
VImage line( VImage, VImage, std::vector<int>, std::vector<int>, std::vector<int>, std::vector<int> ) throw( VError );

// headers for package iofuncs
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
static VImage binfile( char*, int, int, int, int ) throw( VError );
VImage cache( int, int, int ) throw( VError );
char* getext() throw( VError );
int header_get_typeof( char* ) throw( VError );
int header_int( char* ) throw( VError );
double header_double( char* ) throw( VError );
char* header_string( char* ) throw( VError );
char* history_get() throw( VError );
void printdesc() throw( VError );

// headers for package mask
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010

// headers for package morphology
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
double cntlines( int ) throw( VError );
VImage dilate( VIMask ) throw( VError );
VImage rank( int, int, int ) throw( VError );
static VImage rank_image( std::vector<VImage>, int ) throw( VError );
static VImage maxvalue( std::vector<VImage> ) throw( VError );
VImage label_regions( int& ) throw( VError );
VImage zerox( int ) throw( VError );
VImage erode( VIMask ) throw( VError );
VImage profile( int ) throw( VError );

// headers for package mosaicing
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
VImage align_bands() throw( VError );
double correl( VImage, int, int, int, int, int, int, int&, int& ) throw( VError );
int _find_lroverlap( VImage, int, int, int, int, int, int, int, int&, double&, double&, double&, double& ) throw( VError );
int _find_tboverlap( VImage, int, int, int, int, int, int, int, int&, double&, double&, double&, double& ) throw( VError );
VImage global_balance( double ) throw( VError );
VImage global_balancef( double ) throw( VError );
VImage lrmerge( VImage, int, int, int ) throw( VError );
VImage lrmerge1( VImage, int, int, int, int, int, int, int, int, int ) throw( VError );
VImage lrmosaic( VImage, int, int, int, int, int, int, int, int, int ) throw( VError );
VImage lrmosaic1( VImage, int, int, int, int, int, int, int, int, int, int, int, int, int ) throw( VError );
VImage match_linear( VImage, int, int, int, int, int, int, int, int ) throw( VError );
VImage match_linear_search( VImage, int, int, int, int, int, int, int, int, int, int ) throw( VError );
double maxpos_subpel( double& ) throw( VError );
VImage remosaic( char*, char* ) throw( VError );
VImage tbmerge( VImage, int, int, int ) throw( VError );
VImage tbmerge1( VImage, int, int, int, int, int, int, int, int, int ) throw( VError );
VImage tbmosaic( VImage, int, int, int, int, int, int, int, int, int ) throw( VError );
VImage tbmosaic1( VImage, int, int, int, int, int, int, int, int, int, int, int, int, int ) throw( VError );

// headers for package other
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
VImage benchmark() throw( VError );
double benchmark2() throw( VError );
VImage benchmarkn( int ) throw( VError );
static VImage eye( int, int, double ) throw( VError );
static VImage grey( int, int ) throw( VError );
static VImage feye( int, int, double ) throw( VError );
static VImage fgrey( int, int ) throw( VError );
static VImage fzone( int ) throw( VError );
static VImage make_xy( int, int ) throw( VError );
static VImage zone( int ) throw( VError );

// headers for package relational
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
VImage blend( VImage, VImage ) throw( VError );
VImage equal( VImage ) throw( VError );
VImage equal( std::vector<double> ) throw( VError );
VImage equal( double ) throw( VError );
VImage ifthenelse( VImage, VImage ) throw( VError );
VImage less( VImage ) throw( VError );
VImage less( std::vector<double> ) throw( VError );
VImage less( double ) throw( VError );
VImage lesseq( VImage ) throw( VError );
VImage lesseq( std::vector<double> ) throw( VError );
VImage lesseq( double ) throw( VError );
VImage more( VImage ) throw( VError );
VImage more( std::vector<double> ) throw( VError );
VImage more( double ) throw( VError );
VImage moreeq( VImage ) throw( VError );
VImage moreeq( std::vector<double> ) throw( VError );
VImage moreeq( double ) throw( VError );
VImage notequal( VImage ) throw( VError );
VImage notequal( std::vector<double> ) throw( VError );
VImage notequal( double ) throw( VError );

// headers for package resample
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
VImage rightshift_size( int, int, int ) throw( VError );
VImage shrink( double, double ) throw( VError );
VImage stretch3( double, double ) throw( VError );

// headers for package video
// this file automatically generated from
// VIPS library 7.22.1-Tue Jun 22 10:26:51 BST 2010
static VImage video_test( int, int ) throw( VError );
static VImage video_v4l1( char*, int, int, int, int, int, int ) throw( VError );

