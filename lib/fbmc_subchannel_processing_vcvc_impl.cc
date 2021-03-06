/* -*- c++ -*- */
/* 
 * Copyright 2014 <+YOU OR YOUR COMPANY+>.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "fbmc_subchannel_processing_vcvc_impl.h"
#include "malloc16.h"
#include <volk/volk.h>

namespace gr {
	namespace ofdm {

		fbmc_subchannel_processing_vcvc::sptr
		fbmc_subchannel_processing_vcvc::make(unsigned int M, unsigned int syms_per_frame, int sel_preamble, int zero_pads, bool extra_pad, int sel_eq)
		{
			return gnuradio::get_initial_sptr
				(new fbmc_subchannel_processing_vcvc_impl(M, syms_per_frame, sel_preamble, zero_pads, extra_pad, sel_eq));
		}

		/*
		 * The private constructor
		 */
		fbmc_subchannel_processing_vcvc_impl::fbmc_subchannel_processing_vcvc_impl(unsigned int M, unsigned int syms_per_frame, int sel_preamble, int zero_pads, bool extra_pad, int sel_eq)
			: gr::sync_block("fbmc_subchannel_processing_vcvc",
				gr::io_signature::make(1, 1, sizeof(gr_complex)*M),
				gr::io_signature::make(2, 2, sizeof(gr_complex)*M)),
		d_M(M),
		d_syms_per_frame(syms_per_frame),
		d_preamble(),
		d_preamble_2(),
		d_sel_eq(sel_eq),
		d_sel_preamble(sel_preamble),
		d_estimation(M,1),
		d_estimation_1(M,1),
		d_estimation_2(M,1),
		d_eq_coef(3*d_M,1),
		d_conj( static_cast< gr_complex * >( malloc16Align( sizeof( gr_complex ) *M ) ) ),
		d_squared( static_cast< float * >( malloc16Align( sizeof( float ) *M ) ) ),
		d_divide( static_cast< float * >( malloc16Align( sizeof( float ) *M ) ) ),
		//d_norm_vect(d_M,1./2.128),
		d_conj1( static_cast< gr_complex * >( malloc16Align( sizeof( gr_complex ) *M ) ) ),
		d_conj2( static_cast< gr_complex * >( malloc16Align( sizeof( gr_complex ) *M ) ) ),
		d_conj3( static_cast< gr_complex * >( malloc16Align( sizeof( gr_complex ) *M ) ) ),
		d_squared1( static_cast< float * >( malloc16Align( sizeof( float ) *M ) ) ),
		d_divide1( static_cast< float * >( malloc16Align( sizeof( float ) *M ) ) ),
		d_sum1_i( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_sum1_q( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_sum2_i( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_sum2_q( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_sum3_i( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_sum3_q( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_sumc_i( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_sumc_q( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_sum1( static_cast< gr_complex * >( malloc16Align( sizeof( gr_complex ) *M ) ) ),
		d_sum2( static_cast< gr_complex * >( malloc16Align( sizeof( gr_complex ) *M ) ) ),
		d_sum3( static_cast< gr_complex * >( malloc16Align( sizeof( gr_complex ) *M ) ) ),
		d_starte( static_cast< gr_complex * >( malloc16Align( sizeof( gr_complex ) *M ) ) ),
		d_starte_i( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_starte_q( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_start1_i( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_start1_q( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_start2_i( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_start2_q( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_estimation_1_i( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_estimation_1_q( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_estimation_2_i( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_estimation_2_q( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_estimation_i( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),
		d_estimation_q( static_cast< float* >( malloc16Align( sizeof( float ) *M ) ) ),





		d_ones(d_M,1.0),
		ii(0),
		fr(0),
		normalization_factor(1),
		normalization_factor2(1),
		d_zero_pads(zero_pads),
		d_extra_pad(extra_pad)
		{
		    const int alignment_multiple = volk_get_alignment() / sizeof(gr_complex);
		    set_alignment(std::max(1, alignment_multiple));

			if(d_sel_eq==1 || d_sel_eq==2){ //three taps
				set_history(3);
			}

			//prepare preamble
			//prepare center
			if(d_sel_preamble == 0){ // standard one vector center preamble [1,-j,-1,j]
				normalization_factor = 1;
				std::vector<gr_complex> dummy;
				dummy.push_back(gr_complex(1,0));
				dummy.push_back(gr_complex(0,-1));
				dummy.push_back(gr_complex(-1,0));
				dummy.push_back(gr_complex(0,1));				
				for(int i=0;i<(int)(d_M/4);i++){
					d_preamble.insert(d_preamble.end(), dummy.begin(), dummy.end());
				}
				d_preamble_length = d_M*(1+2*d_zero_pads);
				if(d_extra_pad){
					d_preamble_length+=d_M;
				}
			}else if(d_sel_preamble == 1){ // standard preamble with triple repetition
				// normalization_factor = 1;
				normalization_factor = 2.6074;
				std::vector<gr_complex> dummy;
				dummy.push_back(gr_complex(1,0));
				dummy.push_back(gr_complex(-1,0));
				dummy.push_back(gr_complex(1,0));
				dummy.push_back(gr_complex(-1,0));
				for(int i=0;i<(int)(d_M/4);i++){
					d_preamble.insert(d_preamble.end(), dummy.begin(), dummy.end());
				}
				d_preamble_length = d_M*(3+2*d_zero_pads);
				if(d_extra_pad){
					d_preamble_length+=d_M;
				}
			}else if(d_sel_preamble == 2){ // IAM-R preamble [1, -1,-1, 1]
				normalization_factor = 1;
				std::vector<gr_complex> dummy;
				dummy.push_back(gr_complex(1,0));
				dummy.push_back(gr_complex(-1,0));
				dummy.push_back(gr_complex(-1,0));
				dummy.push_back(gr_complex(1,0));
				
				for(int i=0;i<(int)(d_M/4);i++){
					d_preamble.insert(d_preamble.end(), dummy.begin(), dummy.end());
				}
				d_preamble_length = d_M*(1+2*d_zero_pads);
				if(d_extra_pad){
					d_preamble_length+=d_M;
				}
			}else if(d_sel_preamble == 3){ // standard preamble with triple repetition

				float fixed_real_pn1 [4200] = { 1, -1, -1, -1, -1, -1,  1,  1,  1, -1,  1, -1,  1,  1,  1, -1,  1, -1,  1,
				    1, -1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1,  1,  1, -1, -1,  1,
				   -1, -1, -1,  1,  1,  1,  1, -1, -1, -1,  1, -1, -1, -1, -1,  1, -1,  1, -1,
				   -1,  1,  1,  1, -1,  1, -1, -1,  1, -1, -1,  1,  1, -1,  1,  1,  1, -1,  1,
				   -1, -1, -1,  1, -1, -1,  1,  1, -1, -1, -1,  1,  1,  1, -1,  1,  1, -1, -1,
				   -1, -1,  1, -1, -1, -1, -1, -1, -1,  1,  1,  1, -1,  1, -1,  1, -1, -1, -1,
				   -1, -1,  1,  1, -1,  1,  1,  1,  1, -1, -1, -1,  1, -1,  1,  1, -1, -1, -1,
				    1, -1,  1, -1,  1, -1,  1,  1, -1, -1, -1,  1,  1, -1, -1,  1,  1, -1,  1,
				   -1,  1, -1,  1, -1,  1,  1,  1,  1, -1, -1, -1,  1,  1,  1,  1, -1, -1, -1,
				   -1, -1,  1,  1, -1, -1,  1, -1,  1, -1,  1,  1,  1, -1, -1, -1, -1,  1,  1,
				   -1,  1, -1, -1,  1, -1,  1, -1,  1, -1, -1, -1, -1, -1,  1, -1,  1, -1, -1,
				   -1, -1,  1,  1,  1,  1, -1,  1,  1,  1,  1,  1, -1, -1, -1, -1, -1,  1,  1,
				   -1,  1, -1, -1,  1, -1,  1, -1, -1, -1,  1, -1,  1, -1, -1, -1,  1,  1, -1,
				   -1, -1, -1, -1, -1,  1,  1,  1,  1, -1, -1, -1,  1, -1,  1, -1, -1, -1, -1,
				    1, -1, -1,  1,  1, -1,  1, -1,  1, -1, -1, -1, -1,  1, -1,  1, -1, -1, -1,
				   -1,  1, -1,  1, -1, -1,  1,  1, -1,  1,  1, -1, -1, -1,  1,  1,  1,  1,  1,
				   -1, -1,  1, -1,  1,  1, -1, -1, -1,  1, -1,  1, -1, -1,  1,  1, -1,  1, -1,
				   -1, -1,  1, -1, -1,  1, -1, -1, -1,  1, -1,  1,  1, -1,  1, -1,  1, -1,  1,
				    1, -1, -1, -1,  1,  1, -1,  1, -1, -1,  1, -1, -1, -1, -1, -1,  1, -1, -1,
				   -1, -1, -1,  1,  1, -1, -1, -1, -1, -1,  1, -1, -1,  1,  1, -1,  1,  1,  1,
				   -1, -1,  1,  1,  1,  1,  1,  1,  1,  1,  1, -1, -1,  1,  1, -1,  1, -1,  1,
				    1, -1,  1, -1, -1,  1, -1, -1,  1, -1,  1,  1,  1, -1,  1,  1, -1,  1,  1,
				   -1, -1,  1, -1, -1,  1, -1, -1,  1, -1,  1,  1, -1, -1, -1,  1,  1,  1,  1,
				    1, -1,  1,  1, -1, -1,  1, -1,  1,  1, -1, -1, -1,  1,  1, -1, -1,  1,  1,
				    1, -1,  1,  1, -1, -1,  1, -1, -1, -1, -1,  1, -1, -1, -1, -1,  1, -1,  1,
				   -1, -1, -1, -1,  1, -1, -1,  1,  1, -1,  1,  1, -1,  1, -1,  1,  1,  1, -1,
				    1,  1, -1,  1, -1, -1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, -1,
				   -1, -1,  1, -1, -1, -1,  1,  1, -1,  1, -1,  1,  1,  1, -1, -1,  1,  1,  1,
				    1, -1, -1, -1,  1,  1,  1, -1, -1, -1,  1, -1,  1, -1,  1,  1, -1, -1, -1,
				   -1,  1, -1, -1, -1,  1,  1, -1, -1,  1,  1,  1,  1, -1,  1,  1,  1,  1,  1,
				   -1, -1,  1, -1, -1, -1, -1,  1, -1, -1, -1, -1,  1, -1, -1, -1,  1, -1, -1,
				   -1, -1, -1, -1,  1,  1,  1,  1,  1, -1, -1, -1, -1,  1, -1, -1, -1, -1, -1,
				    1,  1, -1,  1, -1, -1, -1, -1,  1,  1, -1,  1,  1,  1, -1, -1, -1, -1,  1,
				    1,  1,  1, -1, -1, -1, -1, -1,  1,  1,  1,  1, -1,  1,  1,  1, -1, -1, -1,
				    1,  1, -1, -1,  1,  1, -1, -1, -1,  1, -1, -1, -1,  1,  1,  1, -1,  1, -1,
				   -1, -1,  1,  1,  1,  1,  1,  1,  1, -1,  1,  1,  1, -1,  1,  1,  1,  1, -1,
				    1, -1, -1,  1, -1, -1,  1,  1, -1,  1,  1, -1,  1,  1, -1,  1, -1, -1,  1,
				    1, -1,  1,  1, -1,  1, -1, -1, -1,  1, -1,  1,  1, -1,  1,  1, -1,  1,  1,
				   -1,  1,  1,  1,  1,  1,  1,  1,  1,  1, -1, -1,  1, -1,  1,  1, -1, -1,  1,
				    1,  1, -1, -1,  1, -1, -1, -1,  1, -1,  1,  1, -1,  1,  1, -1, -1, -1,  1,
				   -1,  1,  1,  1, -1, -1,  1,  1, -1, -1,  1, -1, -1, -1,  1, -1,  1,  1,  1,
				   -1,  1, -1, -1,  1, -1,  1,  1,  1,  1, -1, -1,  1,  1,  1,  1, -1,  1,  1,
				    1, -1, -1, -1,  1, -1,  1, -1,  1, -1,  1, -1,  1,  1,  1,  1,  1, -1, -1,
				   -1, -1,  1, -1, -1, -1,  1,  1,  1, -1, -1,  1, -1,  1, -1,  1,  1, -1, -1,
				    1, -1,  1,  1, -1, -1, -1,  1, -1,  1, -1, -1, -1,  1,  1, -1, -1,  1, -1,
				    1,  1, -1, -1, -1, -1, -1,  1,  1,  1,  1, -1, -1,  1,  1, -1, -1,  1,  1,
				    1,  1, -1, -1, -1, -1,  1, -1,  1, -1,  1, -1, -1, -1, -1,  1,  1, -1,  1,
				   -1,  1, -1, -1,  1, -1,  1, -1,  1,  1, -1, -1, -1,  1, -1,  1, -1,  1,  1,
				    1,  1, -1, -1, -1, -1, -1, -1,  1, -1,  1,  1, -1, -1, -1, -1,  1,  1,  1,
				   -1, -1,  1,  1,  1,  1, -1,  1, -1,  1,  1,  1, -1, -1, -1, -1, -1, -1,  1,
				   -1,  1, -1, -1,  1,  1,  1,  1,  1,  1, -1,  1, -1, -1, -1,  1,  1, -1,  1,
				   -1,  1, -1, -1,  1, -1,  1, -1,  1, -1,  1,  1, -1, -1,  1,  1,  1,  1, -1,
				   -1,  1,  1, -1, -1, -1,  1, -1, -1,  1,  1, -1,  1,  1, -1, -1, -1,  1, -1,
				   -1, -1,  1, -1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1,  1,  1, -1,  1,  1,
				    1, -1, -1, -1, -1,  1, -1,  1, -1, -1, -1, -1,  1, -1,  1, -1, -1,  1, -1,
				   -1,  1,  1, -1, -1,  1, -1, -1,  1, -1, -1,  1, -1, -1, -1, -1,  1, -1,  1,
				    1,  1,  1, -1, -1,  1, -1,  1,  1,  1, -1, -1,  1,  1, -1,  1, -1,  1, -1,
				   -1, -1, -1, -1,  1,  1, -1, -1,  1,  1,  1, -1,  1, -1, -1,  1,  1, -1, -1,
				    1, -1, -1, -1, -1,  1,  1, -1, -1,  1, -1,  1,  1,  1,  1, -1, -1, -1, -1,
				   -1,  1, -1,  1, -1, -1,  1,  1,  1, -1,  1, -1,  1,  1,  1,  1,  1, -1, -1,
				    1,  1, -1,  1,  1, -1,  1,  1,  1, -1, -1, -1,  1,  1,  1, -1,  1, -1,  1,
				   -1,  1,  1, -1, -1,  1,  1,  1, -1, -1, -1, -1,  1,  1, -1,  1, -1,  1, -1,
				    1, -1,  1, -1,  1, -1,  1,  1, -1,  1, -1,  1, -1,  1,  1,  1, -1, -1,  1,
				   -1, -1,  1,  1,  1, -1, -1,  1,  1, -1,  1, -1,  1,  1,  1,  1, -1,  1,  1,
				   -1, -1, -1, -1, -1, -1,  1,  1,  1,  1, -1,  1,  1,  1,  1,  1, -1, -1,  1,
				    1,  1,  1, -1, -1,  1, -1,  1,  1, -1, -1,  1, -1, -1, -1,  1, -1, -1,  1,
				   -1, -1,  1, -1,  1,  1, -1,  1, -1, -1, -1, -1, -1, -1,  1,  1, -1,  1, -1,
				   -1, -1,  1, -1,  1,  1, -1,  1, -1, -1,  1, -1,  1,  1,  1, -1,  1,  1,  1,
				   -1, -1,  1, -1, -1, -1, -1, -1, -1,  1, -1, -1,  1, -1, -1, -1,  1,  1,  1,
				   -1, -1, -1, -1, -1, -1, -1,  1,  1,  1, -1,  1, -1, -1,  1,  1, -1, -1, -1,
				   -1, -1,  1,  1,  1,  1, -1,  1,  1, -1, -1, -1,  1,  1,  1, -1,  1,  1,  1,
				   -1, -1, -1, -1,  1, -1, -1, -1, -1, -1,  1,  1, -1,  1, -1,  1,  1, -1, -1,
				    1,  1,  1, -1, -1,  1, -1,  1,  1,  1,  1,  1, -1, -1, -1, -1, -1,  1,  1,
				    1,  1,  1, -1, -1, -1, -1, -1,  1,  1, -1,  1, -1,  1, -1, -1, -1, -1,  1,
				    1, -1,  1,  1, -1, -1,  1,  1,  1, -1, -1, -1,  1,  1, -1, -1, -1, -1,  1,
				    1, -1,  1,  1, -1, -1, -1, -1, -1, -1,  1,  1, -1, -1, -1,  1,  1,  1, -1,
				    1, -1,  1, -1, -1, -1,  1,  1, -1, -1, -1, -1,  1,  1, -1,  1,  1, -1,  1,
				    1,  1, -1,  1,  1, -1, -1,  1, -1, -1, -1,  1,  1, -1,  1, -1,  1,  1, -1,
				    1, -1, -1, -1, -1, -1, -1,  1, -1, -1,  1,  1, -1,  1, -1,  1, -1,  1, -1,
				    1, -1, -1,  1,  1, -1, -1, -1,  1,  1,  1,  1,  1,  1, -1, -1,  1,  1,  1,
				   -1, -1,  1,  1,  1,  1,  1, -1,  1,  1, -1,  1,  1,  1,  1, -1,  1, -1,  1,
				   -1,  1, -1,  1,  1,  1,  1,  1, -1, -1,  1, -1,  1, -1, -1, -1,  1, -1,  1,
				    1, -1,  1,  1, -1,  1,  1,  1,  1, -1, -1,  1,  1, -1, -1,  1, -1, -1,  1,
				    1, -1, -1, -1, -1,  1, -1,  1, -1, -1,  1,  1,  1,  1,  1,  1, -1, -1, -1,
				   -1,  1, -1,  1,  1,  1,  1,  1,  1, -1,  1,  1, -1,  1,  1, -1,  1, -1, -1,
				   -1,  1,  1,  1,  1,  1, -1, -1,  1, -1, -1,  1,  1,  1,  1,  1,  1,  1,  1,
				    1, -1,  1, -1, -1, -1, -1, -1, -1, -1, -1,  1, -1,  1,  1, -1,  1, -1, -1,
				   -1,  1,  1, -1,  1,  1, -1,  1,  1,  1, -1,  1,  1, -1,  1,  1,  1,  1,  1,
				    1, -1, -1,  1,  1, -1,  1, -1, -1, -1,  1, -1, -1,  1,  1, -1,  1, -1,  1,
				   -1, -1,  1, -1, -1,  1,  1,  1,  1,  1,  1,  1, -1,  1,  1, -1, -1,  1, -1,
				   -1, -1,  1,  1,  1, -1,  1,  1, -1,  1, -1, -1, -1, -1, -1,  1,  1, -1, -1,
				    1,  1,  1, -1, -1, -1, -1, -1,  1,  1, -1, -1, -1, -1, -1,  1, -1,  1, -1,
				    1,  1,  1,  1, -1, -1,  1,  1, -1, -1,  1,  1, -1,  1, -1, -1,  1, -1,  1,
				   -1, -1,  1, -1,  1, -1,  1, -1,  1,  1, -1,  1,  1, -1,  1,  1, -1, -1, -1,
				   -1,  1, -1,  1, -1,  1, -1,  1, -1, -1, -1,  1,  1, -1,  1,  1, -1, -1, -1,
				    1,  1, -1,  1,  1,  1, -1, -1, -1,  1,  1,  1,  1, -1, -1,  1,  1, -1,  1,
				    1, -1,  1,  1,  1,  1, -1, -1, -1,  1, -1, -1,  1,  1, -1, -1, -1, -1,  1,
				   -1, -1,  1,  1,  1,  1,  1,  1,  1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1,
				    1,  1,  1, -1,  1, -1, -1,  1, -1,  1,  1,  1,  1,  1, -1, -1,  1,  1,  1,
				    1,  1, -1, -1, -1, -1,  1,  1,  1,  1,  1, -1,  1,  1, -1,  1, -1,  1, -1,
				    1,  1,  1, -1,  1,  1,  1, -1,  1, -1,  1,  1,  1,  1, -1, -1,  1, -1, -1,
				    1, -1, -1,  1,  1,  1,  1, -1, -1, -1, -1, -1,  1, -1, -1, -1,  1, -1,  1,
				   -1, -1, -1,  1,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  1,  1,  1, -1, -1,
				   -1, -1, -1,  1,  1, -1, -1,  1, -1,  1, -1,  1, -1, -1,  1, -1, -1,  1,  1,
				   -1, -1, -1,  1,  1, -1,  1,  1,  1,  1,  1,  1,  1,  1, -1,  1, -1, -1, -1,
				   -1, -1,  1,  1, -1, -1, -1, -1,  1,  1,  1,  1, -1, -1,  1, -1, -1,  1,  1,
				   -1,  1, -1,  1, -1, -1, -1,  1,  1, -1, -1, -1,  1,  1,  1, -1, -1,  1, -1,
				    1,  1,  1, -1, -1,  1,  1, -1, -1, -1, -1, -1,  1, -1, -1, -1, -1, -1,  1,
				    1, -1, -1, -1, -1,  1,  1,  1, -1, -1,  1, -1,  1,  1, -1, -1, -1, -1,  1,
				    1, -1,  1, -1, -1,  1,  1,  1, -1,  1,  1, -1,  1,  1,  1, -1, -1, -1,  1,
				   -1, -1,  1,  1,  1,  1,  1,  1,  1,  1, -1, -1,  1, -1, -1, -1,  1, -1,  1,
				   -1, -1,  1,  1, -1, -1,  1,  1,  1,  1, -1, -1,  1,  1,  1, -1,  1, -1, -1,
				   -1, -1, -1,  1, -1,  1,  1,  1,  1, -1, -1, -1,  1,  1, -1, -1, -1,  1, -1,
				   -1, -1, -1,  1,  1,  1,  1,  1, -1, -1,  1,  1, -1,  1,  1,  1,  1,  1, -1,
				    1, -1, -1, -1, -1,  1,  1, -1,  1, -1,  1,  1, -1, -1, -1,  1, -1,  1,  1,
				   -1,  1,  1,  1,  1,  1,  1,  1,  1,  1, -1,  1,  1, -1, -1,  1, -1, -1, -1,
				    1, -1, -1, -1, -1,  1,  1, -1, -1, -1, -1,  1,  1, -1,  1, -1,  1,  1,  1,
				   -1, -1, -1,  1, -1, -1, -1,  1, -1, -1,  1,  1,  1,  1, -1, -1, -1,  1,  1,
				    1, -1,  1,  1,  1,  1, -1,  1, -1, -1, -1,  1, -1, -1, -1,  1, -1,  1, -1,
				    1, -1,  1, -1,  1, -1,  1, -1,  1, -1, -1,  1, -1,  1, -1,  1,  1,  1, -1,
				    1,  1, -1,  1, -1, -1,  1,  1, -1,  1,  1, -1,  1, -1, -1, -1,  1, -1,  1,
				   -1,  1,  1,  1, -1,  1, -1,  1,  1, -1,  1,  1,  1, -1, -1, -1,  1, -1,  1,
				   -1, -1, -1, -1, -1, -1,  1, -1,  1, -1,  1,  1, -1, -1, -1,  1, -1,  1, -1,
				    1, -1, -1, -1, -1,  1, -1, -1,  1,  1,  1,  1,  1,  1, -1, -1,  1, -1,  1,
				    1, -1,  1,  1,  1,  1, -1,  1, -1,  1, -1,  1,  1, -1,  1, -1,  1, -1,  1,
				   -1,  1, -1,  1,  1, -1,  1, -1, -1, -1, -1, -1,  1, -1,  1, -1, -1,  1,  1,
				   -1,  1,  1, -1,  1,  1,  1,  1,  1,  1, -1,  1, -1,  1,  1,  1,  1, -1, -1,
				    1,  1, -1,  1, -1,  1, -1,  1,  1,  1, -1, -1,  1,  1,  1,  1,  1,  1,  1,
				    1, -1,  1, -1, -1, -1,  1, -1,  1,  1,  1,  1,  1, -1, -1,  1, -1,  1,  1,
				   -1, -1, -1,  1,  1,  1, -1,  1, -1, -1,  1, -1, -1, -1, -1, -1,  1,  1, -1,
				    1, -1, -1, -1, -1, -1,  1,  1,  1,  1,  1,  1,  1,  1,  1, -1,  1,  1,  1,
				   -1, -1,  1, -1, -1,  1, -1,  1,  1, -1, -1,  1, -1,  1,  1, -1, -1,  1, -1,
				   -1, -1,  1,  1, -1, -1, -1,  1,  1,  1, -1, -1, -1,  1, -1, -1, -1,  1,  1,
				    1,  1, -1,  1, -1, -1,  1, -1,  1,  1,  1,  1, -1,  1, -1, -1, -1, -1, -1,
				    1, -1, -1,  1,  1, -1, -1, -1,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  1,
				   -1,  1, -1,  1,  1,  1, -1, -1, -1, -1, -1,  1, -1, -1,  1, -1, -1,  1, -1,
				   -1, -1, -1, -1,  1,  1,  1, -1,  1,  1,  1,  1, -1, -1,  1,  1, -1,  1, -1,
				   -1, -1,  1,  1,  1,  1,  1,  1,  1, -1,  1, -1, -1,  1,  1, -1, -1, -1, -1,
				   -1, -1, -1,  1, -1, -1,  1,  1, -1, -1,  1,  1,  1,  1,  1,  1, -1, -1,  1,
				   -1,  1, -1,  1,  1,  1,  1, -1, -1, -1,  1,  1, -1, -1,  1,  1, -1, -1, -1,
				   -1, -1,  1, -1, -1,  1,  1, -1,  1, -1, -1,  1,  1, -1, -1,  1,  1,  1,  1,
				   -1,  1, -1,  1,  1,  1,  1, -1,  1, -1, -1, -1, -1, -1,  1, -1,  1, -1, -1,
				    1, -1,  1,  1, -1,  1,  1, -1,  1, -1,  1,  1,  1,  1,  1,  1, -1,  1, -1,
				    1, -1,  1,  1,  1,  1, -1, -1, -1, -1,  1, -1,  1, -1, -1,  1, -1,  1, -1,
				   -1, -1, -1, -1, -1,  1, -1,  1,  1, -1, -1, -1, -1, -1,  1,  1,  1,  1,  1,
				   -1,  1,  1,  1,  1, -1,  1,  1, -1,  1,  1, -1, -1, -1, -1, -1, -1,  1,  1,
				    1, -1,  1,  1, -1, -1,  1, -1,  1, -1,  1, -1,  1,  1, -1,  1,  1,  1, -1};






				//normalization_factor = 1.41421356;//2.6074;
				//normalization_factor = 3.010577831579845; //(1 + 2*0.5644)*sqrt(2)
				normalization_factor = 2.1288; //(1 + 2*0.5644)
				normalization_factor2 = 2*1.41421356;//2.6074;
/*				std::vector<gr_complex> dummy;
				dummy.push_back(gr_complex(1,0));
				dummy.push_back(gr_complex(-1,0));
				dummy.push_back(gr_complex(1,0));
				dummy.push_back(gr_complex(-1,0));*/
				for(int i=0;i<(int)(d_M/2);i++){
					//d_preamble.insert(d_preamble.end(), dummy.begin(), dummy.end());
					//d_preamble.push_back(fixed_real_pn1[i]);
					//d_preamble.push_back(gr_complex(0,0));
					d_preamble_1.push_back(fixed_real_pn1[i%(d_M/4)]);
					d_preamble_1.push_back(0.0);
					d_preamble_2.push_back(0.0);
					d_preamble_2.push_back(gr_complex(0,fixed_real_pn1[(i+d_M/4	)%(d_M/2)]));
					//d_preamble_2.push_back(gr_complex(fixed_real_pn1[i],0));

				}
				/*for(int i=0;i<(int)(d_M);i++){
				std::cout<<d_preamble_1[i]<<std::endl;
				std::cout<<d_preamble_2[i]<<std::endl;
				}*/
				d_preamble_length = d_M*(3+2*d_zero_pads);
				if(d_extra_pad){
					d_preamble_length+=d_M;
				}
				d_norm_vect2.resize(d_M,-1.0/normalization_factor2);
			}else{ // standard one vector center preamble [1,-j,-1,j]
				normalization_factor = 1;
				std::vector<gr_complex> dummy;
				dummy.push_back(gr_complex(0,1));
				dummy.push_back(gr_complex(-1,0));
				dummy.push_back(gr_complex(0,-1));
				dummy.push_back(gr_complex(1,0));
				
				for(int i=0;i<(int)(d_M/4);i++){
					d_preamble.insert(d_preamble.end(), dummy.begin(), dummy.end());
				}
				d_preamble_length = d_M*(1+2*d_zero_pads);
				if(d_extra_pad){
					d_preamble_length+=d_M;
				}
			}

			d_frame_length=d_preamble_length+2*syms_per_frame*d_M;
			//estimation_point=(((int(d_preamble_length/d_M)-1)/2)+1)*d_M;
			estimation_point=(((int(d_preamble_length/d_M)-1)/2)+1);
			//std::cout<<preamble.size()<<std::endl;
			d_norm_vect.resize(d_M,1.0/normalization_factor);


			//d_norm_vect.resize(d_M,1./normalization_factor);
			// if(extra_pad){
			// 	estimation_point = estimation_point;
			// }

			// equalizer_data.open ("../../matlab/sp_equ_cpp_output.txt",std::ios::out); //|std::ios::app
			// estimation_data.open("../../matlab/sp_est_cpp_output.txt",std::ios::out);
			
			// equalizer_data<<"M="<<d_M<<"\n";
			// equalizer_data<<"syms_per_frame="<<d_syms_per_frame<<"\n";
			// equalizer_data<<"sel_eq="<<d_sel_eq<<"\n";
			// equalizer_data<<"preamble_length="<<d_preamble_length<<"\n";

			// estimation_data<<"M="<<d_M<<"\n";
			// estimation_data<<"syms_per_frame="<<d_syms_per_frame<<"\n";
			// estimation_data<<"sel_eq="<<d_sel_eq<<"\n";
			// estimation_data<<"preamble_length="<<d_preamble_length<<"\n";
			// estimation_data<<"estimation_point="<<estimation_point<<"\n";
			// // equalizer_data<<"estimation="<<d_estimation<<"\n";
			// // std::cout<<preamble.size()<<std::endl;
			// // for(int i=0;i<3*d_M;i++) std::cout<<d_eq_coef[i]<<std::endl;
			// for(int i=0;i<d_M;i++) std::cout<<i<<"\t"<<d_preamble[i]<<std::endl;

		}

		/*
		 * Our virtual destructor.
		 */
		fbmc_subchannel_processing_vcvc_impl::~fbmc_subchannel_processing_vcvc_impl()
		{
			// equalizer_data.close();
			// estimation_data.close();
			if( d_conj )
					    	    free16Align( d_conj );
			if( d_squared )
					    	    free16Align( d_squared );
			if( d_divide )
					    	    free16Align( d_divide );
			if( d_conj1 )
					    	    free16Align( d_conj1 );
			if( d_squared1 )
					    	    free16Align( d_squared1 );
			if( d_divide1 )
					    	    free16Align( d_divide1 );
			if( d_sum1 )
								    	    free16Align( d_sum1 );
			if( d_sum2 )
								    	    free16Align( d_sum2 );
			if( d_sum3 )
								    	    free16Align( d_sum3 );
			if( d_sum1_i )
								    	    free16Align( d_sum1_i );
			if( d_sum1_q )
								    	    free16Align( d_sum1_q );
			if( d_sum2_i )
								    	    free16Align( d_sum2_i );
			if( d_sum2_q )
								    	    free16Align( d_sum2_q );
			if( d_sum3_i )
								    	    free16Align( d_sum3_i );
			if( d_sum3_q )
								    	    free16Align( d_sum3_q );
			if( d_sumc_i )
								    	    free16Align( d_sumc_i );
			if( d_sumc_q )
								    	    free16Align( d_sumc_q );
			if( d_starte_i)
											free16Align( d_starte_i);
			if( d_starte_q)
											free16Align( d_starte_q);
			if( d_start1_i)
											free16Align( d_start1_i);
			if( d_start1_q)
											free16Align( d_start1_q);
			if( d_start2_i)
											free16Align( d_start2_i);
			if( d_start2_q)
											free16Align( d_start2_q);
			if( d_estimation_1_i)
											free16Align( d_estimation_1_i);
			if( d_estimation_1_q)
											free16Align( d_estimation_1_q);
			if( d_estimation_2_i)
											free16Align( d_estimation_2_i);
			if( d_estimation_2_q)
											free16Align( d_estimation_2_q);
			if( d_estimation_i)
											free16Align( d_estimation_i);
			if( d_estimation_q)
											free16Align( d_estimation_q);
		}

		void 
		fbmc_subchannel_processing_vcvc_impl::get_estimation(const gr_complex * start)
		{
			// int offset = estimation_point - d_M+1;
/*			for(int i=0;i<d_M;i++){
				d_estimation[i] = *(start-d_M+i+1)/(d_preamble[i]*normalization_factor);//*gr_complex(0.6863,0));
				// // *(start-d_M+i+1) = d_estimation[i];
				// //logging
				// estimation_data<<"fr "<<fr<<"\t"<<i<<"\t"<<*(start-d_M+i+1)<<"\t"<<(d_preamble[i+d_M])<<"\t"<<d_estimation[i]<<"\t"<<((abs(d_estimation[i])-abs(*(start-d_M+i+1)))>0?"TR":"FA")<<"\n";
				
			}*/

			 //std::cout<<"normalization_factor: "<<normalization_factor<<std::endl;
				//d_estimation[i] = *(start-d_M+i+1)/(d_preamble[i]*normalization_factor);//*gr_complex(0.6863,0));
				// // *(start-d_M+i+1) = d_estimation[i];
				// //logging
				// estimation_data<<"fr "<<fr<<"\t"<<i<<"\t"<<*(start-d_M+i+1)<<"\t"<<(d_preamble[i+d_M])<<"\t"<<d_estimation[i]<<"\t"<<((abs(d_estimation[i])-abs(*(start-d_M+i+1)))>0?"TR":"FA")<<"\n";
			//d_norm_vect.resize(d_M,1.0/normalization_factor);



				// True estimator
				/*volk_32fc_x2_multiply_conjugate_32fc(&d_conj[0],start, &d_preamble[0],d_M);
				volk_32fc_magnitude_squared_32f(&d_squared[0],&d_preamble[0],d_M);
				volk_32f_x2_divide_32f(&d_divide[0],&d_norm_vect[0],&d_squared[0],d_M);
				volk_32fc_32f_multiply_32fc(&d_estimation[0],&d_conj[0],&d_divide[0],d_M);
				*/

				volk_32fc_32f_multiply_32fc(&d_conj[0],start,&d_preamble_1[0],d_M);
				volk_32fc_32f_multiply_32fc(&d_estimation[0],&d_conj[0],&d_norm_vect[0],d_M);

/*
				std::cout<<"FRAME: "<<std::endl;
				for(int i=0;i<d_M/2;i++){
				//	std::cout<<d_estimation[i]<<std::endl;
					d_estimation[2*i+1 ]= d_estimation[2*i];


				}

				for(int i=0;i<d_M;i++){
								std::cout<<d_estimation[i]<<std::endl;


							}
*/

		/*		volk_32fc_deinterleave_32f_x2(&d_start1_i[0],&d_start1_q[0],start-d_M,d_M);
				volk_32fc_deinterleave_32f_x2(&d_start2_i[0],&d_start2_q[0],start+d_M,d_M);
				volk_32f_x2_add_32f(&d_starte_i[0],&d_start1_i[0],&d_start2_i[0],d_M);
				volk_32f_x2_add_32f(&d_starte_q[0],&d_start1_q[0],&d_start2_q[0],d_M);
				volk_32f_x2_interleave_32fc(&d_starte[0],&d_starte_i[0],&d_starte_q[0],d_M);

				volk_32fc_x2_multiply_32fc(&d_conj2[0],&d_starte[0],&d_preamble_2[0],d_M);
				volk_32fc_32f_multiply_32fc(&d_estimation_2[0],&d_conj2[0],&d_norm_vect2[0],d_M);




				volk_32fc_deinterleave_32f_x2(&d_estimation_1_i[0],&d_estimation_1_q[0],&d_estimation_1[0],d_M);
				volk_32fc_deinterleave_32f_x2(&d_estimation_2_i[0],&d_estimation_2_q[0],&d_estimation_2[0],d_M);
				volk_32f_x2_add_32f(&d_estimation_i[0],&d_estimation_1_i[0],&d_estimation_2_i[0],d_M);
				volk_32f_x2_add_32f(&d_estimation_q[0],&d_estimation_1_q[0],&d_estimation_2_q[0],d_M);
				volk_32f_x2_interleave_32fc(&d_estimation[0],&d_estimation_i[0],&d_estimation_q[0],d_M);
*/
				//std::cout<<"FRAME: "<<std::endl;
				for(int i=0;i<d_M/2;i++){
				//	std::cout<<d_estimation[i]<<std::endl;
					d_estimation[2*i+1 ]= d_estimation[2*i];


				}

/*				for(int i=0;i<d_M;i++){
								std::cout<<d_estimation[i]<<std::endl;


							}*/

			// // equalizer_data<<"----------------------------------"<<"\n";
			//fr++;
		}

		void 
		fbmc_subchannel_processing_vcvc_impl::get_equalizer_coefficients(int order){
			for(int i=0;i<d_M;i++){
				// std::cout<<i<<"\t"<<fmod(i-1,d_M)<<"\t"<<fmod(i+1,d_M)<<"\tbefor"<<std::endl;
				// std::cout<<i<<"\t"<<d_estimation[i]<<"\t"<<d_estimation[fmod(i-1,d_M)]<<"\t"<<d_estimation[fmod(i+1,d_M)]<<"\tbefor"<<std::endl;
				gr_complex EQi = gr_complex(1,0)/d_estimation[i];
				//gr_complex EQmin = gr_complex(1,0)/d_estimation[fmod(i-1+d_M,d_M)];
				//gr_complex EQplus = gr_complex(1,0)/d_estimation[fmod(i+1+d_M,d_M)];
				gr_complex EQmin = gr_complex(1,0)/d_estimation[(i-1+d_M)%d_M];
				gr_complex EQplus = gr_complex(1,0)/d_estimation[(i+1+d_M)%d_M];

				gr_complex EQ1, EQ2;
				if(d_sel_eq==1){ // linear interpolation
					EQ1 = (EQmin+EQi)/gr_complex(2,0);
					EQ2 = (EQplus+EQi)/gr_complex(2,0);
				}else if(d_sel_eq==2){
					float ro = 0.5;
					EQ1 = (gr_complex)EQmin*pow((EQi/EQmin),ro);
	                EQ2 = (gr_complex)EQi*pow((EQplus/EQi),ro);
				}

				// 0:2-> new implementation, 3< -> old implementation
				if(order == 0){
					d_eq_coef[i+2*d_M]= pow(gr_complex(-1,0),i)*((EQ1-gr_complex(2,0)*EQi+EQ2)-gr_complex(0,1)*(EQ2-EQ1))/gr_complex(4,0); // change this first
				}else if (order == 1){
					d_eq_coef[i+d_M]= (gr_complex)(EQ1+EQ2)/gr_complex(2,0); // then this
				}else if(order == 2){
					d_eq_coef[i]= pow(gr_complex(-1,0),i)*((EQ1-gr_complex(2,0)*EQi+EQ2)+gr_complex(0,1)*(EQ2-EQ1))/gr_complex(4,0); // lastly this
				}else{
					d_eq_coef[i]= pow(gr_complex(-1,0),i)*((EQ1-gr_complex(2,0)*EQi+EQ2)+gr_complex(0,1)*(EQ2-EQ1))/gr_complex(4,0);
					d_eq_coef[i+d_M]= (gr_complex)(EQ1+EQ2)/gr_complex(2,0);
					d_eq_coef[i+2*d_M]= pow(gr_complex(-1,0),i)*((EQ1-gr_complex(2,0)*EQi+EQ2)-gr_complex(0,1)*(EQ2-EQ1))/gr_complex(4,0);
				}

				// //logging
				// equalizer_data<<(fr-1)<<"\t"<<i<<"\t"<<real(d_estimation[i])<<"\t"<<imag(d_estimation[i])<<"j\t";//((imag(d_estimation[i])>0)?"+":"-")
				// equalizer_data<<real(d_eq_coef[i])<<"\t"<<imag(d_eq_coef[i])<<"j\t";
				// equalizer_data<<real(d_eq_coef[i+d_M])<<"\t"<<imag(d_eq_coef[i+d_M])<<"j\t";
				// equalizer_data<<real(d_eq_coef[i+2*d_M])<<"\t"<<imag(d_eq_coef[i+2*d_M])<<"j\n"; //<<"fr " est: "
			}
			// fr++;
		}

		int
		fbmc_subchannel_processing_vcvc_impl::work(int noutput_items,
				gr_vector_const_void_star &input_items,
				gr_vector_void_star &output_items)
		{
			const gr_complex *in = (const gr_complex *) input_items[0];
			gr_complex *out = (gr_complex *) output_items[0];
			gr_complex *out_estimation = (gr_complex *) output_items[1];

			//gr_complex estimout = 1;
			//int low, size;
			//low = 0;
			//size = noutput_items*d_M;

			//volk_32fc_x2_multiply_conjugate_32fc(out, in, &d_estimation[low%(int)(d_M)], size);
			//volk_32fc_s32fc_multiply_32fc(out_estimation, &d_estimation[low%(int)(d_M)],estimout, size);


			//out_estimation[i] = d_estimation[i%(int)(d_M)];


			//if(ii%d_frame_length == estimation_point)
										//get_estimation(in+low);
			//ii++;
			// Do <+signal processing+>
			ii = ii%int(d_frame_length/d_M);
			for(int i=0;i<noutput_items;i++){
				
				if(d_sel_eq==0){
					// one tap zero forcing equalizer
/*
					out[i] = in[i]/(d_estimation[i%(int)(d_M)]); //sumfactor?????
					out_estimation[i] = d_estimation[i%(int)(d_M)];
*/

					volk_32fc_x2_multiply_conjugate_32fc(&d_conj1[0],&in[i*d_M],&d_estimation[0],d_M);
					volk_32fc_magnitude_squared_32f(&d_squared1[0],&d_estimation[0],d_M);
					volk_32f_x2_divide_32f(&d_divide1[0],&d_ones[0],&d_squared1[0],d_M);
					volk_32fc_32f_multiply_32fc(&out[i*d_M],&d_conj1[0],&d_divide1[0],d_M);

					memcpy(&out_estimation[i*d_M], &d_estimation[0], sizeof(gr_complex)*d_M);



					// std::cout<<out[i]<<"\t"<<d_estimation[i%d_M]<<"\t"<<in[i]<<std::endl;
				}else if(d_sel_eq==1 || d_sel_eq==2){
					// three taps with linear(=1) or geometric(=2) interpolation
					//out[i] = (d_eq_coef[i%d_M]*in[i+2*d_M]+d_eq_coef[(i%d_M)+d_M]*in[i+d_M]+d_eq_coef[(i%d_M)+2*d_M]*in[i]);///gr_complex(3,0);

					volk_32fc_x2_multiply_32fc(&d_sum1[0],&in[(i+2)*d_M],&d_eq_coef[0],d_M);
					volk_32fc_x2_multiply_32fc(&d_sum2[0],&in[(i+1)*d_M],&d_eq_coef[d_M],d_M);
					volk_32fc_x2_multiply_32fc(&d_sum3[0],&in[(i)*d_M],&d_eq_coef[2*d_M],d_M);

					volk_32fc_deinterleave_32f_x2(&d_sum1_i[0],&d_sum1_q[0],&d_sum1[0],d_M);
					volk_32fc_deinterleave_32f_x2(&d_sum2_i[0],&d_sum2_q[0],&d_sum2[0],d_M);
					volk_32fc_deinterleave_32f_x2(&d_sum3_i[0],&d_sum3_q[0],&d_sum3[0],d_M);

					volk_32f_x2_add_32f(&d_sumc_i[0],&d_sum1_i[0],&d_sum2_i[0],d_M);
					volk_32f_x2_add_32f(&d_sumc_i[0],&d_sumc_i[0],&d_sum3_i[0],d_M);
					volk_32f_x2_add_32f(&d_sumc_q[0],&d_sum1_q[0],&d_sum2_q[0],d_M);
					volk_32f_x2_add_32f(&d_sumc_q[0],&d_sumc_q[0],&d_sum3_q[0],d_M);

					volk_32f_x2_interleave_32fc(&out[(i)*d_M],&d_sumc_i[0],&d_sumc_q[0],d_M);


					//out_estimation[i] = d_estimation[i%(int)(d_M)];
					memcpy(&out_estimation[i*d_M], &d_estimation[0], sizeof(gr_complex)*d_M);
				}else{
					// no equalization
					//out[i] = in[i];
					//out_estimation[i] = d_estimation[i%(int)(d_M)];
					memcpy(&out[i*d_M], &in[i*d_M], sizeof(gr_complex)*d_M);
					memcpy(&out_estimation[i*d_M], &d_estimation[0], sizeof(gr_complex)*d_M);
				}

				//std::cout<<ii<<std::endl;
				// old implementation
				if(d_sel_eq<3){
					if(d_sel_eq == 1 || d_sel_eq == 2){
						if(ii%int(d_frame_length/d_M)  == estimation_point-1){
							//get_estimation(in+i+2*d_M);
							get_estimation(in+(i+2)*d_M);
							get_equalizer_coefficients(999);
						}
					}
					else if(d_sel_eq==0){
						if(ii%int(d_frame_length/d_M) == estimation_point-1){
							//get_estimation(in+i);
							get_estimation(in+(i)*d_M);
						}
					}
					ii++;
				}

				// new implementation, we dont change the coefficients immediately after they are generated, instead we will use the old 
				// coeffs until samples from previous symbol is processed.
				// if(d_sel_eq<3){
				// 	if(d_sel_eq == 1 || d_sel_eq == 2){
				// 		if(ii%d_frame_length==(2*d_M-1)){
				// 			get_estimation(in+i+2*d_M);
				// 			get_equalizer_coefficients(0);
				// 		}
				// 		else if(ii%d_frame_length==(2*d_M-1)+d_M){
				// 			get_equalizer_coefficients(1);
				// 		}else if(ii%d_frame_length==(2*d_M-1)+2*d_M){
				// 			get_equalizer_coefficients(2);
				// 		}
				// 	}
				// 	else if(d_sel_eq==0){
				// 		if(ii%d_frame_length==(2*d_M-1)){
				// 			get_estimation(in+i);
				// 		}
				// 	}
				// 	ii++;
				// }
			}

			// Tell runtime system how many output items we produced.
			return noutput_items;
		}

	} /* namespace ofdm */
} /* namespace gr */

